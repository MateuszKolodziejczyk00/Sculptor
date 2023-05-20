#include "DDGISceneSubsystem.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "EngineFrame.h"

namespace spt::rsc
{

namespace parameters
{

RendererBoolParameter ddgiEnabled("Enable DDGI", {"DDGI"}, true);
RendererFloatParameter ddgiBlendHysteresis("DDGI Blend Hysteresis", { "DDGI" }, 0.97f, 0.f, 1.f);

} // parameters

DDGISceneSubsystem::DDGISceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_probesDebugMode(EDDDGIProbesDebugMode::None)
	, m_probesUpdatedPerFrame(math::Vector3u::Zero())
	, m_requiresClearingData(false)
{
	InitializeDDGIParameters();

	InitializeTextures();

	m_ddgiDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIDS>(RENDERER_RESOURCE_NAME("DDGIDS"), rdr::EDescriptorSetStateFlags::Persistent);
	m_ddgiDS->u_ddgiParams					= GetDDGIParams();
	m_ddgiDS->u_probesIrradianceTexture		= GetProbesIrradianceTexture();
	m_ddgiDS->u_probesHitDistanceTexture	= GetProbesHitDistanceTexture();
}

DDGIUpdateProbesGPUParams DDGISceneSubsystem::CreateUpdateProbesParams() const
{
	const math::Vector3u blocksNum = math::Utils::DivideCeil(GetProbesVolumeResolution(), m_probesUpdatedPerFrame);

	math::Vector3u updateCoords(lib::rnd::Random<Uint32>(0, blocksNum.x()),
								lib::rnd::Random<Uint32>(0, blocksNum.y()),
								lib::rnd::Random<Uint32>(0, blocksNum.z()));

	updateCoords = updateCoords.cwiseProduct(m_probesUpdatedPerFrame);
	updateCoords.x() = std::min(updateCoords.x(), GetProbesVolumeResolution().x() - m_probesUpdatedPerFrame.x());
	updateCoords.y() = std::min(updateCoords.y(), GetProbesVolumeResolution().y() - m_probesUpdatedPerFrame.y());
	updateCoords.z() = std::min(updateCoords.z(), GetProbesVolumeResolution().z() - m_probesUpdatedPerFrame.z());

	DDGIUpdateProbesGPUParams params;
	params.probesToUpdateCoords	= updateCoords;
	params.probesToUpdateCount	= m_probesUpdatedPerFrame;
	params.probeRaysMaxT		= 100.f;
	params.probeRaysMinT		= 0.01f;
	params.raysNumPerProbe		= GetRaysNumPerProbe();
	params.probesNumToUpdate	= params.probesToUpdateCount.x() * params.probesToUpdateCount.y() * params.probesToUpdateCount.z();
	params.rcpRaysNumPerProbe	= 1.f / static_cast<Real32>(params.raysNumPerProbe);
	params.rcpProbesNumToUpdate	= 1.f / static_cast<Real32>(params.probesNumToUpdate);
	params.skyIrradiance		= math::Vector3f(0.52f, 0.81f, 0.92f) * 0.05f;
	params.groundIrradiance		= math::Vector3f::Constant(0.1f) * 0.05f;
	params.blendHysteresis		= parameters::ddgiBlendHysteresis;
	
	params.raysRotation							= math::Matrix4f::Identity();
	params.raysRotation.topLeftCorner<3, 3>()	= lib::rnd::RandomRotationMatrix();

	return params;
}

const lib::SharedPtr<rdr::TextureView>& DDGISceneSubsystem::GetProbesIrradianceTexture() const
{
	return m_probesIrradianceTextureView;
}

const lib::SharedPtr<rdr::TextureView>& DDGISceneSubsystem::GetProbesHitDistanceTexture() const
{
	return m_probesHitDistanceTextureView;
}

const DDGIGPUParams& DDGISceneSubsystem::GetDDGIParams() const
{
	return m_ddgiParams;
}

void DDGISceneSubsystem::SetProbesDebugMode(EDDDGIProbesDebugMode::Type mode)
{
	m_probesDebugMode = mode;
}

EDDDGIProbesDebugMode::Type DDGISceneSubsystem::GetProbesDebugMode() const
{
	return m_probesDebugMode;
}

const lib::SharedPtr<DDGIDS>& DDGISceneSubsystem::GetDDGIDS() const
{
	return m_ddgiDS;
}

Bool DDGISceneSubsystem::IsDDGIEnabled() const
{
	return parameters::ddgiEnabled;
}

Bool DDGISceneSubsystem::RequiresClearingData() const
{
	return m_requiresClearingData;
}

void DDGISceneSubsystem::PostClearingData()
{
	m_requiresClearingData = false;
}

Uint32 DDGISceneSubsystem::GetRaysNumPerProbe() const
{
	return 256;
}

Uint32 DDGISceneSubsystem::GetProbesNum() const
{
	return m_ddgiParams.probesVolumeResolution.x() * m_ddgiParams.probesVolumeResolution.y() * m_ddgiParams.probesVolumeResolution.z();
}

math::Vector3u DDGISceneSubsystem::GetProbesVolumeResolution() const
{
	return m_ddgiParams.probesVolumeResolution;
}

math::Vector2u DDGISceneSubsystem::GetProbeIrradianceDataRes() const
{
	return m_ddgiParams.probeIrradianceDataRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeIrradianceWithBorderDataRes() const
{
	return m_ddgiParams.probeIrradianceDataWithBorderRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeDistancesDataRes() const
{
	return m_ddgiParams.probeHitDistanceDataRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeDistancesDataWithBorderRes() const
{
	return m_ddgiParams.probeHitDistanceDataWithBorderRes;
}

void DDGISceneSubsystem::InitializeDDGIParameters()
{
	const math::Vector3u probesVolumeRes = math::Vector3u::Constant(22u);

	m_probesUpdatedPerFrame = math::Vector3u::Constant(6u);

	const Uint32 probesTextureWidth		= probesVolumeRes.x() * probesVolumeRes.z();
	const Uint32 probesTextureHeight	= probesVolumeRes.y();

	m_ddgiParams.probesOriginWorldLocation					= math::Vector3f(-5.f, -11.f, -0.5f);
	m_ddgiParams.probesSpacing								= math::Vector3f(0.5f, 1.f, 0.5f);
	m_ddgiParams.probesEndWorldLocation						= m_ddgiParams.probesOriginWorldLocation + probesVolumeRes.cast<Real32>().cwiseProduct(m_ddgiParams.probesSpacing);
	m_ddgiParams.rcpProbesSpacing							= m_ddgiParams.probesSpacing.cwiseInverse();
	m_ddgiParams.probesVolumeResolution						= probesVolumeRes;
	m_ddgiParams.probesWrapCoords							= math::Vector3i::Zero();

	m_ddgiParams.probeIrradianceDataRes						= math::Vector2u::Constant(6u);
	m_ddgiParams.probeIrradianceDataWithBorderRes			= m_ddgiParams.probeIrradianceDataRes + math::Vector2u::Constant(2u);

	m_ddgiParams.probeHitDistanceDataRes					= math::Vector2u::Constant(16u);
	m_ddgiParams.probeHitDistanceDataWithBorderRes			= m_ddgiParams.probeHitDistanceDataRes + math::Vector2u::Constant(2u);

	m_ddgiParams.probesIrradianceTextureRes					= math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(m_ddgiParams.probeIrradianceDataWithBorderRes);
	m_ddgiParams.probesHitDistanceTextureRes				= math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(m_ddgiParams.probeHitDistanceDataWithBorderRes);

	m_ddgiParams.probesIrradianceTexturePixelSize			= m_ddgiParams.probesIrradianceTextureRes.cast<Real32>().cwiseInverse();
	m_ddgiParams.probesIrradianceTextureUVDeltaPerProbe		= m_ddgiParams.probesIrradianceTexturePixelSize.cwiseProduct(m_ddgiParams.probeIrradianceDataWithBorderRes.cast<Real32>());
	m_ddgiParams.probesIrradianceTextureUVPerProbeNoBorder	= m_ddgiParams.probesIrradianceTexturePixelSize.cwiseProduct(m_ddgiParams.probeIrradianceDataRes.cast<Real32>());
	
	m_ddgiParams.probesHitDistanceTexturePixelSize			= m_ddgiParams.probesHitDistanceTextureRes.cast<Real32>().cwiseInverse();
	m_ddgiParams.probesHitDistanceUVDeltaPerProbe			= m_ddgiParams.probesHitDistanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeHitDistanceDataWithBorderRes.cast<Real32>());
	m_ddgiParams.probesHitDistanceTextureUVPerProbeNoBorder	= m_ddgiParams.probesHitDistanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeHitDistanceDataRes.cast<Real32>());
	
	m_ddgiParams.probeIrradianceEncodingGamma				= 1.f;
}

void DDGISceneSubsystem::InitializeTextures()
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesTextureWidth		= m_ddgiParams.probesVolumeResolution.x() * m_ddgiParams.probesVolumeResolution.z();
	const Uint32 probesTextureHeight	= m_ddgiParams.probesVolumeResolution.y();

	const math::Vector2u irradiancePerProbeRes	= GetProbeIrradianceWithBorderDataRes();
	const math::Vector2u distancePerProbeRes	= GetProbeDistancesDataWithBorderRes();

	const math::Vector3u probesRes = GetProbesVolumeResolution();

	const rhi::ETextureUsage texturesUsage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);

	rhi::TextureDefinition probesIrradianceTextureDef;
	probesIrradianceTextureDef.resolution	= math::Vector3u{ probesTextureWidth * irradiancePerProbeRes.x(), probesTextureHeight * irradiancePerProbeRes.y(), 1u };
	probesIrradianceTextureDef.usage		= texturesUsage;
	probesIrradianceTextureDef.format		= rhi::EFragmentFormat::B10G11R11_U_Float;
	const lib::SharedRef<rdr::Texture> probesIrradianceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Probes Irradiance"), probesIrradianceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition probesIrradianceViewDefinition;
	probesIrradianceViewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_probesIrradianceTextureView = probesIrradianceTexture->CreateView(RENDERER_RESOURCE_NAME("Probes Irradiance View"), probesIrradianceViewDefinition);

	rhi::TextureDefinition probesDistanceTextureDef;
	probesDistanceTextureDef.resolution	= math::Vector3u{ probesTextureWidth * distancePerProbeRes.x(), probesTextureHeight * distancePerProbeRes.y(), 1u };
	probesDistanceTextureDef.usage		= texturesUsage;
	probesDistanceTextureDef.format		= rhi::EFragmentFormat::RG16_S_Float;
	const lib::SharedRef<rdr::Texture> probesDistanceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Probes Distance"), probesDistanceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition probesDistanceViewDefinition;
	probesDistanceViewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_probesHitDistanceTextureView = probesDistanceTexture->CreateView(RENDERER_RESOURCE_NAME("Probes Distance View"), probesDistanceViewDefinition);
}

} // spt::rsc
