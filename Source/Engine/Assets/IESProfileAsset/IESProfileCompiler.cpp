#include "IESProfileCompiler.h"
#include "MathUtils.h"


namespace spt::as
{

class IESProfileSampler
{
public:

	explicit IESProfileSampler(const IESProfileDefinition& profileDef)
		: m_profile(profileDef)
	{
		m_cachedCosThetaValues.resize(m_profile.verticalAnglesNum);
		for (Uint32 vAngleIdx = 0; vAngleIdx < m_profile.verticalAnglesNum; ++vAngleIdx)
		{
			const Real32 verticalAngleDeg = m_profile.verticalAngles[vAngleIdx] + 180.f;
			const Real32 verticalAngleRad = math::Utils::DegreesToRadians(verticalAngleDeg);
			m_cachedCosThetaValues[vAngleIdx] = std::cos(verticalAngleRad);
		}

		m_cachedPhiValues.resize(m_profile.horizontalAnglesNum);
		for (Uint32 hAngleIdx = 0; hAngleIdx < m_profile.horizontalAnglesNum; ++hAngleIdx)
		{
			const Real32 horizontalAngleDeg = m_profile.horizontalAngles[hAngleIdx];
			const Real32 horizontalAngleRad = math::Utils::DegreesToRadians(horizontalAngleDeg);
			m_cachedPhiValues[hAngleIdx] = horizontalAngleRad;
		}

		if (profileDef.photometricType == PhotometricType::TypeC)
		{
			if (m_profile.horizontalAngles.back() <= 180.f)
			{
				m_horizontalWrapAround = true;
			}
		}
		else
		{
			SPT_CHECK_NO_ENTRY();
		}
	}

	Real32 SampleCandela(Real32 cosTheta, Real32 phi) const
	{
		if (m_horizontalWrapAround && phi > pi<Real32>)
		{
			phi = 2.f * pi<Real32> - phi;
		}

		const Real32 vAngleIdx = [&]() -> Real32
		{
			for (Uint32 idx = 0; idx < m_profile.verticalAnglesNum - 1u; ++idx)
			{
				if (cosTheta >= m_cachedCosThetaValues[idx] && cosTheta <= m_cachedCosThetaValues[idx + 1u])
				{
					return static_cast<Real32>(idx) + (cosTheta - m_cachedCosThetaValues[idx]) / (m_cachedCosThetaValues[idx + 1u] - m_cachedCosThetaValues[idx]);
				}
			}

			return static_cast<Real32>(m_profile.verticalAnglesNum - 1u);
		}();

		const Real32 hAngleIdx = [&]() -> Real32
		{
			for (Uint32 idx = 0; idx < m_profile.horizontalAnglesNum - 1u; ++idx)
			{
				if (phi >= m_cachedPhiValues[idx] && phi <= m_cachedPhiValues[idx + 1u])
				{
					return static_cast<Real32>(idx) + (phi - m_cachedPhiValues[idx]) / (m_cachedPhiValues[idx + 1u] - m_cachedPhiValues[idx]);
				}
			}

			return static_cast<Real32>(m_profile.horizontalAnglesNum - 1u);
		}();

		const Uint32 vAngleIdx0 = static_cast<Uint32>(std::clamp(std::floor(vAngleIdx), 0.f, static_cast<Real32>(m_profile.verticalAnglesNum - 1u)));
		const Uint32 vAngleIdx1 = static_cast<Uint32>(std::clamp(std::ceil(vAngleIdx), 0.f, static_cast<Real32>(m_profile.verticalAnglesNum - 1u)));
		const Real32 vBilinearFactor = vAngleIdx1 != vAngleIdx0 ? vAngleIdx - vAngleIdx0 : 0.f;

		const Uint32 hAngleIdx0 = static_cast<Uint32>(std::clamp(std::floor(hAngleIdx), 0.f, static_cast<Real32>(m_profile.horizontalAnglesNum - 1u)));
		const Uint32 hAngleIdx1 = static_cast<Uint32>(std::clamp(std::ceil(hAngleIdx), 0.f, static_cast<Real32>(m_profile.horizontalAnglesNum - 1u)));
		const Real32 hBilinearFactor = hAngleIdx1 != hAngleIdx0 ? hAngleIdx - hAngleIdx0 : 0.f;

		const Real32 c00 = m_profile.candela[hAngleIdx0 * m_profile.verticalAnglesNum + vAngleIdx0];
		const Real32 c10 = m_profile.candela[hAngleIdx0 * m_profile.verticalAnglesNum + vAngleIdx1];
		const Real32 c01 = m_profile.candela[hAngleIdx1 * m_profile.verticalAnglesNum + vAngleIdx0];
		const Real32 c11 = m_profile.candela[hAngleIdx1 * m_profile.verticalAnglesNum + vAngleIdx1];

		const Real32 c0 = std::lerp(c00, c10, vBilinearFactor);
		const Real32 c1 = std::lerp(c01, c11, vBilinearFactor);

		return std::lerp(c0, c1, hBilinearFactor);
	}

private:

	IESProfileDefinition m_profile;

	lib::DynamicArray<Real32> m_cachedCosThetaValues;
	lib::DynamicArray<Real32> m_cachedPhiValues;

	Bool m_horizontalWrapAround = false;
};


IESProfileCompilationResult IESProfileCompiler::Compile(IESProfileDefinition profileDef) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_resolution.x() >= 2u && m_resolution.y() >= 2u);
	SPT_CHECK(!profileDef.candela.empty());

	const Uint32 textureSize = m_resolution.x() * m_resolution.y();

	const Real32 maxCandela = *std::max_element(profileDef.candela.begin(), profileDef.candela.end()) * profileDef.candelaMultiplier;
	const Real32 normalizationFactor = maxCandela > 0.f ? (65535.f / maxCandela) : 0.f;

	IESProfileCompilationResult bakedProfile;
	bakedProfile.resolution         = m_resolution;
	bakedProfile.lightSourceCandela = maxCandela;
	bakedProfile.textureData.resize(textureSize);

	const IESProfileSampler sampler(profileDef);

	const Real32 dPhi =  (2.f * pi<Real32>) / static_cast<Real32>(m_resolution.x() - 1u);
	const Real32 dCosTheta = 2.f / static_cast<Real32>(m_resolution.y() - 1u);

	for (Uint32 y = 0; y < m_resolution.y(); ++y)
	{
		const Real32 cosTheta = -1.f + static_cast<Real32>(y) * dCosTheta;
		for (Uint32 x = 0; x < m_resolution.x(); ++x)
		{
			const Real32 phi = static_cast<Real32>(x) * dPhi;
			const Real32 candelaValue = sampler.SampleCandela(cosTheta, phi) * profileDef.candelaMultiplier;

			const Uint16 normalizedValue = static_cast<Uint16>(std::clamp(candelaValue * normalizationFactor, 0.f, 65535.f));

			bakedProfile.textureData[y * m_resolution.x() + x] = normalizedValue;
		}
	}

	return bakedProfile;
}

} // spt::as
