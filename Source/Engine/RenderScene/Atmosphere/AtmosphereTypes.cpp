#include "AtmosphereTypes.h"
#include "MathShapes.h"


namespace spt::rsc
{

namespace atmosphere_utils
{

Real32 GetMiePhase(Real32 cosTheta)
{
	const Real32 g = 0.8f;
	const Real32 scale = 3.0f / (8.0f * math::Pi<Real32>);

	const Real32 g2 = g * g;

	const Real32 num = (1.0f - g2) * (1.0f + cosTheta * cosTheta);
	const Real32 denom = (2.f + g2) * pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f);

	return scale * num / denom;
}


Real32 GetRayleighPhase(Real32 cosTheta)
{
	const Real32 k = 3.f / (16.f * math::Pi<Real32>);
	return k * (1.f + cosTheta * cosTheta);
}


struct ScatteringValues
{
	math::Vector3f rayleighScattering;
	Real32 mieScattering;
	math::Vector3f extinction;
};


ScatteringValues ComputeScatteringValues(const AtmosphereParams& atmosphere, math::Vector3f locationMM)
{
	SPT_MAYBE_UNUSED
		const Real32 altitudeKM = (locationMM.norm() - atmosphere.groundRadiusMM) * 1000.f;

	const Real32 rayleighDensity = exp(-altitudeKM / 8.f);
	const Real32 mieDensity = exp(-altitudeKM / 1.2f);

	ScatteringValues result{};
	result.rayleighScattering = atmosphere.rayleighScattering * rayleighDensity;
	result.mieScattering = atmosphere.mieScattering * mieDensity;

	const Real32 rayleighAbsorption = atmosphere.rayleighAbsorption * rayleighDensity;
	const Real32 mieAbsorption = atmosphere.mieAbsorption * mieDensity;

	const Real32 ozoneDensity = std::max(0.f, 1.f - std::abs(altitudeKM - 25.f) / 15.f);
	const math::Vector3f ozoneAbsorption = atmosphere.ozoneAbsorption * ozoneDensity;

	result.extinction = math::Vector3f::Constant(rayleighAbsorption + mieAbsorption + result.mieScattering) + ozoneAbsorption + result.rayleighScattering;

	return result;
}

math::Vector3f ComputeTransmittance(const AtmosphereParams& atmosphere, math::Vector3f location, math::Vector3f direction, Real32 stepsNum)
{
	const math::Ray rayToSun = math::Ray(location, direction);
	const math::Sphere groundSphere = math::Sphere(math::Vector3f::Zero(), atmosphere.groundRadiusMM);

	if (rayToSun.IntersectSphere(groundSphere))
	{
		return math::Vector3f::Constant(0.f);
	}

	const math::Sphere atmosphereSphere = math::Sphere(math::Vector3f::Zero(), atmosphere.atmosphereRadiusMM);

	const math::IntersectionResult atmosphereIntersection = rayToSun.IntersectSphere(atmosphereSphere);

	const Real32 rcpStepsNum = 1.f / stepsNum;

	Real32 t = 0.f;
	math::Vector3f transmittance = math::Vector3f::Constant(1.f);

	for (Real32 i = 0; i < stepsNum; i += 1.f)
	{
		const Real32 newT = (i + 0.3f) * rcpStepsNum * *atmosphereIntersection;
		const Real32 dt = newT - t;
		t = newT;

		const math::Vector3f currentLocation = rayToSun.origin + rayToSun.direction * t;

		const ScatteringValues scatteringValues = ComputeScatteringValues(atmosphere, currentLocation);

		transmittance.x() *= std::expf(-dt * scatteringValues.extinction.x());
		transmittance.y() *= std::expf(-dt * scatteringValues.extinction.y());
		transmittance.z() *= std::expf(-dt * scatteringValues.extinction.z());
	}

	return transmittance;
}

math::Vector3f ComputeTransmittanceAtZenith(const AtmosphereParams& atmosphere)
{
	const math::Vector3f groundLocation = math::Vector3f(0.f, 0.f, atmosphere.groundRadiusMM + 0.00001f);
	const math::Vector3f transmittanceZenith = ComputeTransmittance(atmosphere, groundLocation, math::Vector3f::UnitZ(), 100.f);
	return transmittanceZenith;
}

DirectionalLightIlluminance ComputeDirectionalLightIlluminanceInAtmosphere(const AtmosphereParams& atmosphere, math::Vector3f lightDirection, math::Vector3f illuminanceGroundAtZenith, Real32 lightConeAngleRadians)
{
	const math::Vector3f groundLocation = math::Vector3f(0.f, 0.f, atmosphere.groundRadiusMM + 0.00001f);
	const math::Vector3f transmittanceZenith = atmosphere.transmittanceAtZenith;

	const math::Vector3f outerSpaceIlluminance = illuminanceGroundAtZenith.cwiseQuotient(transmittanceZenith);

	const math::Vector3f transmittance = ComputeTransmittance(atmosphere, groundLocation, -lightDirection, 100.f);

	return {
		outerSpaceIlluminance,
		outerSpaceIlluminance.cwiseProduct(transmittance)
	};
}

} // atmosphere_utils

} // spt::rsc
