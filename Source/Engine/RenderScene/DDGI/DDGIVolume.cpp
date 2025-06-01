#include "DDGIVolume.h"
#include "DDGIScene.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "View/SceneView.h"
#include "EngineFrame.h"
#include "RenderScene.h"

namespace spt::rsc::ddgi
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priority_statics
{
static constexpr Real32 priorityPerTickMultiplierAfterEverySecondSinceRelit = 6.f;
} // priority_statics

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIRelitZone =================================================================================

DDGIRelitZone::DDGIRelitZone()
{
}

void DDGIRelitZone::Initialize(DDGIVolume& volume, const math::AlignedBox3u& probesBoundingBox)
{
	SPT_CHECK(!m_volume);
	SPT_CHECK(!probesBoundingBox.isEmpty());

	m_volume            = &volume;
	m_probesBoundingBox = probesBoundingBox;
}

DDGIVolume& DDGIRelitZone::GetVolume() const
{
	SPT_CHECK(!!m_volume);
	return *m_volume;
}

const math::AlignedBox3u& DDGIRelitZone::GetProbesBoundingBox() const
{
	return m_probesBoundingBox;
}

math::AlignedBox3f DDGIRelitZone::GetBoundingBox() const
{
	const DDGIVolume& volume = GetVolume();

	return volume.GetZoneAABB(m_probesBoundingBox);
}

void DDGIRelitZone::MarkSunLightDirectionDirty()
{
	m_isSunLightDirectionDirty = true;
}

void DDGIRelitZone::UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime)
{
	const Real32 priorityFactor = ComputeRelitPriorityFactor(sceneView, currentTime, deltaTime);
	
	const Real32 priority = GetVolume().GetVolumePriority() * priorityFactor;

	m_currentRelitPriority += priority * deltaTime;

	if (IsOverlappingInvalidProbes())
	{
		m_forceRelitNextFrame = true;
	}
}

Real32 DDGIRelitZone::GetRelitHysteresisDelta() const
{
	Real32 hysteresisDelta = 0.f;
	if (m_isSunLightDirectionDirty)
	{
		const DDGIScene& ddgiScene = GetVolume().GetDDGIScene();
		const engn::FrameContext& currentFrame = ddgiScene.GetOwningScene().GetCurrentFrameRef();
		const Real32 currentTime = currentFrame.GetTime();
		const Real32 timeSinceRelit = currentTime - m_lastRelitTime;
		hysteresisDelta -= std::min(std::max(timeSinceRelit - 0.08f, 0.f) * 1.2f, 1.f);
	}

	return hysteresisDelta;
}

void DDGIRelitZone::PostRelit()
{
	DDGIVolume& volume = GetVolume();

	const RenderScene& renderScene = volume.GetDDGIScene().GetOwningScene();
	m_lastRelitTime = renderScene.GetCurrentFrameRef().GetTime();

	m_currentRelitPriority     = 0.f;
	m_isSunLightDirectionDirty = false;
	m_forceRelitNextFrame      = false;
}

Real32 DDGIRelitZone::ComputeRelitPriorityFactor(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime) const
{
	const math::AlignedBox3f zoneAABB = GetBoundingBox();

	Real32 priorityFactor = 1.f;

	const Real32 secondsSinceRelit = currentTime - m_lastRelitTime;
	priorityFactor += secondsSinceRelit * priority_statics::priorityPerTickMultiplierAfterEverySecondSinceRelit;

	return priorityFactor;
}

Bool DDGIRelitZone::IsOverlappingInvalidProbes() const
{
	const DDGIVolume& volume = GetVolume();

	if (volume.MovedSinceLastRelit())
	{
		const math::AlignedBox3f prevAABB = volume.GetPrevAABB();
		const math::AlignedBox3f& currentAABB = volume.GetVolumeAABB();

		math::AlignedBox3f validProbesAABB = prevAABB.intersection(currentAABB);
		validProbesAABB.min() -= math::Vector3f::Constant(0.01f);
		validProbesAABB.max() += math::Vector3f::Constant(0.01f);

		const math::AlignedBox3f zoneAABB = GetBoundingBox();

		if (!validProbesAABB.contains(zoneAABB))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIVolumeZones ===============================================================================

DDGIVolumeZones::DDGIVolumeZones()
{
}

void DDGIVolumeZones::Initialize(DDGIVolume& volume, const math::Vector3u& zoneSize)
{
	SPT_CHECK(m_zones.empty());

	const math::Vector3u& probesResolution = volume.GetProbesVolumeResolution();

	const math::Vector3u zonesNum = math::Utils::DivideCeil(probesResolution, zoneSize);

	m_zones.reserve(zonesNum.x() * zonesNum.y() * zonesNum.z());

	for (Uint32 z = 0u; z < zonesNum.z(); ++z)
	{
		for (Uint32 y = 0u; y < zonesNum.y(); ++y)
		{
			for (Uint32 x = 0u; x < zonesNum.x(); ++x)
			{
				const math::Vector3u zoneMin = math::Vector3u(x, y, z).cwiseProduct(zoneSize);
				const math::Vector3u zoneMax = (zoneMin + zoneSize).cwiseMin(probesResolution);

				DDGIRelitZone& newZone = m_zones.emplace_back();
				newZone.Initialize(volume, math::AlignedBox3u(zoneMin, zoneMax));
			}
		}
	}
}

void DDGIVolumeZones::MarkSunLightDirectionDirty()
{
	for (DDGIRelitZone& zone : m_zones)
	{
		zone.MarkSunLightDirectionDirty();
	}
}

void DDGIVolumeZones::UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime)
{
	SPT_PROFILER_FUNCTION();

	for (DDGIRelitZone& zone : m_zones)
	{
		zone.UpdateRelitPriority(sceneView, currentTime, deltaTime);
	}
}

void DDGIVolumeZones::CollectZonesToRelit(DDGIZonesCollector& zonesCollector)
{
	SPT_PROFILER_FUNCTION();

	for (DDGIRelitZone& zone : m_zones)
	{
		zonesCollector.Collect(zone);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIZonesCollector ============================================================================

DDGIZonesCollector::DDGIZonesCollector(SizeType zonesToRelitBudget)
	: m_zonesToRelitBudget(zonesToRelitBudget)
{
}

void DDGIZonesCollector::Collect(DDGIRelitZone& zone)
{
	const auto priorityComparator = [](const DDGIRelitZone* lhs, const DDGIRelitZone* rhs)
	{
		const auto createZoneCompareData = [](const DDGIRelitZone* zone) { return std::make_tuple(zone->WantsForceRelitNextFrame(), zone->GetCurrentRelitPriority()); };
		return createZoneCompareData(lhs) > createZoneCompareData(rhs);
	};

	const auto it = std::lower_bound(m_zonesToRelit.begin(), m_zonesToRelit.end(), &zone, priorityComparator);

	if (zone.WantsForceRelitNextFrame() || it != m_zonesToRelit.end() || m_zonesToRelit.size() < m_zonesToRelitBudget)
	{
		m_zonesToRelit.insert(it, &zone);
	}

	if (m_zonesToRelit.size() > m_zonesToRelitBudget && !m_zonesToRelit.back()->WantsForceRelitNextFrame())
	{
		m_zonesToRelit.pop_back();
	}
}

const lib::DynamicArray<DDGIRelitZone*>& DDGIZonesCollector::GetZonesToRelit() const
{
	return m_zonesToRelit;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIVolume ====================================================================================

DDGIVolume::DDGIVolume(DDGIScene& owningScene)
	: m_owningScene(owningScene)
	, m_volumeAABB{}
	, m_prevAABB(math::AlignedBox3f(math::Vector3f::Constant(999.9f), math::Vector3f::Constant(999.9f)))
	, m_requiresInvalidation(false)
{
	m_owningScene.RegisterVolume(*this);
}

DDGIVolume::~DDGIVolume()
{
	SPT_PROFILER_FUNCTION();

	if (m_gpuVolumeHandle.IsValid())
	{
		m_gpuVolumeHandle.Destroy();
	}

	m_owningScene.UnregisterVolume(*this);
}

void DDGIVolume::Initialize(DDGIGPUVolumeHandle gpuVolumeHandle, const DDGIVolumeParams& inParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_state == EDDGIVolumeState::Uninitialized);
	SPT_CHECK(gpuVolumeHandle.IsValid());

	const DDGIVolumeGPUParams& gpuParams = gpuVolumeHandle.GetGPUParams();

	m_volumeAABB = math::AlignedBox3f(gpuParams.probesOriginWorldLocation, gpuParams.probesEndWorldLocation);

	m_gpuVolumeHandle = std::move(gpuVolumeHandle);

	m_volumePriority = inParams.priority;

	m_state = EDDGIVolumeState::Ready;

	m_relitZones.Initialize(*this, inParams.relitZoneResolution);
	
	m_requiresInvalidation = false;
}

Bool DDGIVolume::Translate(const math::Translation3f& requestedTranslation)
{
	SPT_CHECK(IsReady());

	Bool translated = false;

	math::Vector3i wrapDelta = math::Vector3i::Zero();
	
	const DDGIVolumeGPUParams& gpuParams = m_gpuVolumeHandle.GetGPUParams();
	wrapDelta = requestedTranslation.vector().cwiseProduct(gpuParams.rcpProbesSpacing).cast<Int32>();

	if (wrapDelta != math::Vector3i::Zero())
	{
		TranslateImpl(wrapDelta);

		translated = true;
	}

	return translated;
}

void DDGIVolume::TeleportTo(const math::Vector3f& newAABBMin)
{
	SPT_CHECK(IsReady());

	const math::AlignedBox3f newAABB(newAABBMin, newAABBMin + m_volumeAABB.sizes());

	// invalid, because we need to invalidate all probes
	m_prevAABB = math::AlignedBox3f(math::Vector3f::Constant(1.f), math::Vector3f::Constant(0.f));

	m_requiresInvalidation = true;

	m_volumeAABB = newAABB;

	DDGIVolumeGPUParams& gpuParams = m_gpuVolumeHandle.GetGPUParamsMutable();

	gpuParams.probesOriginWorldLocation = m_volumeAABB.min();
	gpuParams.probesEndWorldLocation    = m_volumeAABB.max();

	gpuParams.probesWrapCoords = math::Vector3i::Zero();
}

void DDGIVolume::MarkSunLightDirectionDirty()
{
	SPT_CHECK(IsReady());

	m_relitZones.MarkSunLightDirectionDirty();
}

math::AlignedBox3f DDGIVolume::GetPrevAABB() const
{
	return m_prevAABB.value_or(m_volumeAABB);
}

math::AlignedBox3f DDGIVolume::GetZoneAABB(const math::AlignedBox3u& probesAABB) const
{
	const DDGIVolumeGPUParams& gpuParams = m_gpuVolumeHandle.GetGPUParams();

	const math::Vector3f probesSpacing = gpuParams.probesSpacing;

	const math::Vector3f min = gpuParams.probesOriginWorldLocation + probesSpacing.cwiseProduct(probesAABB.min().cast<Real32>());
	const math::Vector3f max = gpuParams.probesOriginWorldLocation + probesSpacing.cwiseProduct(probesAABB.max().cast<Real32>() - math::Vector3f::Constant(1.f));

	return math::AlignedBox3f(min, max);
}

Bool DDGIVolume::RequiresInvalidation() const
{
	return m_requiresInvalidation;
}

void DDGIVolume::PostInvalidation()
{
	m_requiresInvalidation = false;
	m_prevAABB.reset();
}

Bool DDGIVolume::MovedSinceLastRelit() const
{
	return m_prevAABB.has_value();
}

math::AlignedBox3f DDGIVolume::GetVolumePrevAABB() const
{
	return m_prevAABB.value_or(m_volumeAABB);
}

DDGIScene& DDGIVolume::GetDDGIScene() const
{
	return m_owningScene;
}

EDDGIVolumeState DDGIVolume::GetState() const
{
	return m_state;
}

Bool DDGIVolume::IsReady() const
{
	return GetState() == EDDGIVolumeState::Ready;
}

Uint32 DDGIVolume::GetVolumeIdx() const
{
	SPT_CHECK(IsReady());
	return m_gpuVolumeHandle.GetVolumeIdx();
}

const math::AlignedBox3f& DDGIVolume::GetVolumeAABB() const
{
	SPT_CHECK(IsReady());
	return m_volumeAABB;
}

math::Vector3f DDGIVolume::GetVolumeCenter() const
{
	SPT_CHECK(IsReady());
	return m_volumeAABB.center();
}

Real32 DDGIVolume::GetVolumePriority() const
{
	return m_volumePriority;
}

Uint32 DDGIVolume::GetProbeDataTexturesNum() const
{
	return m_gpuVolumeHandle.GetProbesDataTexturesNum();
}

lib::SharedPtr<rdr::TextureView> DDGIVolume::GetProbesIlluminanceTexture(Uint32 textureIdx) const
{
	return m_gpuVolumeHandle.GetProbesIlluminanceTexture(textureIdx);
}

lib::SharedPtr<rdr::TextureView> DDGIVolume::GetProbesHitDistanceTexture(Uint32 textureIdx) const
{
	return m_gpuVolumeHandle.GetProbesHitDistanceTexture(textureIdx);
}

lib::SharedPtr<rdr::TextureView> DDGIVolume::GetProbesAverageLuminanceTexture() const
{
	return m_gpuVolumeHandle.GetProbesAverageLuminanceTexture();
}

const DDGIVolumeGPUParams& DDGIVolume::GetVolumeGPUParams() const
{
	SPT_CHECK(IsReady());
	return m_gpuVolumeHandle.GetGPUParams();
}

Uint32 DDGIVolume::GetProbesNum() const
{
	SPT_CHECK(IsReady());
	const DDGIVolumeGPUParams& gpuParams = m_gpuVolumeHandle.GetGPUParams();
	return gpuParams.probesVolumeResolution.x() * gpuParams.probesVolumeResolution.y() * gpuParams.probesVolumeResolution.z();
}

const math::Vector3u& DDGIVolume::GetProbesVolumeResolution() const
{
	SPT_CHECK(IsReady());
	return m_gpuVolumeHandle.GetGPUParams().probesVolumeResolution;
}

void DDGIVolume::UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime)
{
	SPT_CHECK(IsReady());

	m_relitZones.UpdateRelitPriority(sceneView, currentTime, deltaTime);
}

void DDGIVolume::CollectZonesToRelit(DDGIZonesCollector& zonesCollector)
{
	SPT_CHECK(IsReady());

	m_relitZones.CollectZonesToRelit(zonesCollector);
}

void DDGIVolume::TranslateImpl(const math::Vector3i& wrapDelta)
{
	if (!m_prevAABB)
	{
		m_prevAABB = m_volumeAABB;
	}

	m_requiresInvalidation = true;

	DDGIVolumeGPUParams& gpuParams = m_gpuVolumeHandle.GetGPUParamsMutable();

	const math::Vector3f translationToApply(wrapDelta.cast<Real32>().cwiseProduct(gpuParams.probesSpacing));

	m_volumeAABB.translate(translationToApply);

	gpuParams.probesOriginWorldLocation += translationToApply;
	gpuParams.probesEndWorldLocation    += translationToApply;

	const auto wrapCoords = [](Int32 coord, Int32 resolution)
	{
		return coord < 0 ? resolution - (-coord % resolution) : coord % resolution;
	};

	math::Vector3i newWrapCoords = (gpuParams.probesWrapCoords + wrapDelta);
	newWrapCoords.x() = wrapCoords(newWrapCoords.x(), gpuParams.probesVolumeResolution.x());
	newWrapCoords.y() = wrapCoords(newWrapCoords.y(), gpuParams.probesVolumeResolution.y());
	newWrapCoords.z() = wrapCoords(newWrapCoords.z(), gpuParams.probesVolumeResolution.z());
	
	gpuParams.probesWrapCoords = newWrapCoords;
}

} // spt::rsc::ddgi