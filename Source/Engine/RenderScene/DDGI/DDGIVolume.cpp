#include "DDGIVolume.h"
#include "DDGIScene.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "View/SceneView.h"

namespace spt::rsc::ddgi
{

namespace priority_statics
{
static constexpr Real32 deltaPriorityAfterMove        = 50.f;
static constexpr Real32 deltaPriorityAfterTeleport    = 60.f;
static constexpr Real32 deltaPriorityAfterGlobalRelit = 10.f;
} // priority_statics

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

	m_volumeAABB                     = math::AlignedBox3f(gpuParams.probesOriginWorldLocation, gpuParams.probesEndWorldLocation);
	m_cachedProbesIlluminanceTexture = gpuVolumeHandle.GetProbesIlluminanceTexture();
	m_cachedProbesHitDistanceTexture = gpuVolumeHandle.GetProbesHitDistanceTexture();

	m_gpuVolumeHandle = std::move(gpuVolumeHandle);

	m_volumePriority = inParams.priority;

	m_state = EDDGIVolumeState::Ready;

	m_relitPriority = 0.f;
	
	m_requiresInvalidation = false;

	m_wantsGlobalRelit = false;
}

Bool DDGIVolume::Translate(const math::Translation3f& requestedTranslation)
{
	SPT_CHECK(IsReady());

	Bool translated = false;

	math::Vector3i wrapDelta = math::Vector3i::Zero();
	
	{
		const DDGIVolumeGPUParams& gpuParams = m_gpuVolumeHandle.GetGPUParams();

		wrapDelta = requestedTranslation.vector().cwiseProduct(gpuParams.rcpProbesSpacing).cast<Int32>();
	}

	if (wrapDelta != math::Vector3i::Zero())
	{
		TranslateImpl(wrapDelta);

		m_relitPriority += priority_statics::deltaPriorityAfterMove;

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

	m_relitPriority += priority_statics::deltaPriorityAfterTeleport;
}

void DDGIVolume::PostVolumeRelit()
{
	SPT_CHECK(IsReady());

	m_prevAABB.reset();

	m_relitPriority = 0.f;
}

void DDGIVolume::RequestGlobalRelit()
{
	SPT_CHECK(IsReady());

	m_wantsGlobalRelit = true;

	m_relitPriority += priority_statics::deltaPriorityAfterGlobalRelit;
}

math::AlignedBox3f DDGIVolume::GetPrevAABB() const
{
	return m_prevAABB.value_or(m_volumeAABB);
}

Bool DDGIVolume::RequiresInvalidation() const
{
	return m_requiresInvalidation;
}

void DDGIVolume::PostInvalidation()
{
	m_requiresInvalidation = false;
}

Bool DDGIVolume::WantsFullVolumeRelit() const
{
	return MovedSinceLastRelit() || m_wantsGlobalRelit;
}

Bool DDGIVolume::MovedSinceLastRelit() const
{
	return m_prevAABB.has_value();
}

DDGIScene& DDGIVolume::GetOwningScene() const
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

const lib::SharedPtr<rdr::TextureView>& DDGIVolume::GetProbesIlluminanceTexture() const
{
	return m_cachedProbesIlluminanceTexture;
}

const lib::SharedPtr<rdr::TextureView>& DDGIVolume::GetProbesHitDistanceTexture() const
{
	return m_cachedProbesHitDistanceTexture;
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

void DDGIVolume::UpdateRelitPriority(const SceneView& sceneView, Real32 deltaTime)
{
	SPT_CHECK(IsReady());

	Real32 priorityFactor = 1.f;
	if (m_volumeAABB.contains(sceneView.GetLocation()))
	{
		priorityFactor += 1.f;
	}
	else
	{
		const math::Vector3f cameraToVolume = (m_volumeAABB.center() - sceneView.GetLocation()).normalized();

		priorityFactor += cameraToVolume.dot(sceneView.GetForwardVector());
	}
	
	const Real32 priority = m_volumePriority * priorityFactor;

	m_relitPriority += priority * deltaTime;
}

Real32 DDGIVolume::GetRelitPriority() const
{
	return m_relitPriority;
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

	const auto wrapCoords = [](int coord, int resolution)
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