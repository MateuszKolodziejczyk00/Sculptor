#include "DDGISceneSubsystem.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "EngineFrame.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "YAMLSerializerHelper.h"
#include "ConfigUtils.h"

namespace spt::srl
{

template<>
struct TypeSerializer<rsc::DDGIConfig>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("ProbesVolumeResolution", data.probesVolumeResolution);
		serializer.Serialize("LocalProbesUpdateResolution", data.localProbesUpdateResolution);
		serializer.Serialize("ProbesSpacing", data.probesSpacing);
		serializer.Serialize("LocalUpdateRaysPerProbe", data.localUpdateRaysPerProbe);
		serializer.Serialize("GlobalUpdateRaysPerProbe", data.globalUpdateRaysPerProbe);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::rsc::DDGIConfig)

namespace spt::rsc
{

namespace parameters
{

RendererBoolParameter ddgiEnabled("Enable DDGI", {"DDGI"}, true);
RendererFloatParameter ddgiBlendHysteresisForLocalUpdate("DDGI Blend Hysteresis (Local)", { "DDGI" }, 0.95f, 0.f, 1.f);
RendererFloatParameter ddgiBlendHysteresisForGlobalUpdate("DDGI Blend Hysteresis (Global)", { "DDGI" }, 0.96f, 0.f, 1.f);

} // parameters

DDGISceneSubsystem::DDGISceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_debugMode(EDDGIDebugMode::None)
	, m_requiresClearingData(false)
	, m_wantsGlobalUpdate(false)
{
	engn::ConfigUtils::LoadConfigData(m_config, "DDGIConfig.yaml");

	InitializeDDGIParameters();

	InitializeTextures();

	m_ddgiDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIDS>(RENDERER_RESOURCE_NAME("DDGIDS"), rdr::EDescriptorSetStateFlags::Persistent);
	m_ddgiDS->u_ddgiParams					= GetDDGIParams();
	m_ddgiDS->u_probesIlluminanceTexture	= GetProbesIlluminanceTexture();
	m_ddgiDS->u_probesHitDistanceTexture	= GetProbesHitDistanceTexture();

	RenderSceneRegistry& registry = owningScene.GetRegistry();

	registry.on_construct<DirectionalLightData>().connect<&DDGISceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().connect<&DDGISceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().connect<&DDGISceneSubsystem::OnDirectionalLightUpdated>(this);
}

DDGISceneSubsystem::~DDGISceneSubsystem()
{
	RenderSceneRegistry& registry = GetOwningScene().GetRegistry();

	registry.on_construct<DirectionalLightData>().disconnect<&DDGISceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().disconnect<&DDGISceneSubsystem::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().disconnect<&DDGISceneSubsystem::OnDirectionalLightUpdated>(this);
}

DDGIUpdateProbesGPUParams DDGISceneSubsystem::CreateUpdateProbesParams() const
{
	const math::Vector3u probesToUpdate = m_wantsGlobalUpdate ? m_ddgiParams.probesVolumeResolution : m_config.localProbesUpdateResolution;

	const math::Vector3u blocksNum = math::Utils::DivideCeil(GetProbesVolumeResolution(), probesToUpdate);

	math::Vector3u updateCoords(lib::rnd::Random<Uint32>(0, blocksNum.x()),
								lib::rnd::Random<Uint32>(0, blocksNum.y()),
								lib::rnd::Random<Uint32>(0, blocksNum.z()));

	updateCoords = updateCoords.cwiseProduct(probesToUpdate);
	updateCoords.x() = std::min(updateCoords.x(), GetProbesVolumeResolution().x() - probesToUpdate.x());
	updateCoords.y() = std::min(updateCoords.y(), GetProbesVolumeResolution().y() - probesToUpdate.y());
	updateCoords.z() = std::min(updateCoords.z(), GetProbesVolumeResolution().z() - probesToUpdate.z());

	DDGIUpdateProbesGPUParams params;
	params.probesToUpdateCoords			= updateCoords;
	params.probesToUpdateCount			= probesToUpdate;
	params.probeRaysMaxT				= 100.f;
	params.probeRaysMinT				= 0.0f;
	params.raysNumPerProbe				= GetRaysNumPerProbe();
	params.probesNumToUpdate			= params.probesToUpdateCount.x() * params.probesToUpdateCount.y() * params.probesToUpdateCount.z();
	params.rcpRaysNumPerProbe			= 1.f / static_cast<Real32>(params.raysNumPerProbe);
	params.rcpProbesNumToUpdate			= 1.f / static_cast<Real32>(params.probesNumToUpdate);
	params.blendHysteresis				= m_wantsGlobalUpdate ? parameters::ddgiBlendHysteresisForGlobalUpdate : parameters::ddgiBlendHysteresisForLocalUpdate;
	params.illuminanceDiffThreshold		= 2000.f;
	params.luminanceDiffThreshold		= 500.f;
	
	params.raysRotation							= math::Matrix4f::Identity();
	params.raysRotation.topLeftCorner<3, 3>()	= lib::rnd::RandomRotationMatrix();

	return params;
}

const lib::SharedPtr<rdr::TextureView>& DDGISceneSubsystem::GetProbesIlluminanceTexture() const
{
	return m_probesIlluminanceTextureView;
}

const lib::SharedPtr<rdr::TextureView>& DDGISceneSubsystem::GetProbesHitDistanceTexture() const
{
	return m_probesHitDistanceTextureView;
}

const DDGIGPUParams& DDGISceneSubsystem::GetDDGIParams() const
{
	return m_ddgiParams;
}

void DDGISceneSubsystem::SetDebugMode(EDDGIDebugMode::Type mode)
{
	m_debugMode = mode;
}

EDDGIDebugMode::Type DDGISceneSubsystem::GetDebugMode() const
{
	return m_debugMode;
}

const lib::MTHandle<DDGIDS>& DDGISceneSubsystem::GetDDGIDS() const
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

void DDGISceneSubsystem::PostUpdateProbes()
{
	m_wantsGlobalUpdate = false;
}

Uint32 DDGISceneSubsystem::GetRaysNumPerProbe() const
{
	return m_wantsGlobalUpdate ? m_config.globalUpdateRaysPerProbe : m_config.localUpdateRaysPerProbe;
}

Uint32 DDGISceneSubsystem::GetProbesNum() const
{
	return m_ddgiParams.probesVolumeResolution.x() * m_ddgiParams.probesVolumeResolution.y() * m_ddgiParams.probesVolumeResolution.z();
}

math::Vector3u DDGISceneSubsystem::GetProbesVolumeResolution() const
{
	return m_ddgiParams.probesVolumeResolution;
}

math::Vector2u DDGISceneSubsystem::GetProbeIlluminanceDataRes() const
{
	return m_ddgiParams.probeIlluminanceDataRes;
}

math::Vector2u DDGISceneSubsystem::GetProbeIlluminanceWithBorderDataRes() const
{
	return m_ddgiParams.probeIlluminanceDataWithBorderRes;
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
	const math::Vector3u probesVolumeRes = m_config.probesVolumeResolution;

	const Uint32 probesTextureWidth		= probesVolumeRes.x() * probesVolumeRes.z();
	const Uint32 probesTextureHeight	= probesVolumeRes.y();

	m_ddgiParams.probesOriginWorldLocation					= math::Vector3f(-5.f, -11.f, -0.99f);
	m_ddgiParams.probesSpacing								= m_config.probesSpacing;
	m_ddgiParams.probesEndWorldLocation						= m_ddgiParams.probesOriginWorldLocation + probesVolumeRes.cast<Real32>().cwiseProduct(m_ddgiParams.probesSpacing);
	m_ddgiParams.rcpProbesSpacing							= m_ddgiParams.probesSpacing.cwiseInverse();
	m_ddgiParams.probesVolumeResolution						= probesVolumeRes;
	m_ddgiParams.probesWrapCoords							= math::Vector3i::Zero();

	m_ddgiParams.probeIlluminanceDataRes					= math::Vector2u::Constant(6u);
	m_ddgiParams.probeIlluminanceDataWithBorderRes			= m_ddgiParams.probeIlluminanceDataRes + math::Vector2u::Constant(2u);

	m_ddgiParams.probeHitDistanceDataRes					= math::Vector2u::Constant(16u);
	m_ddgiParams.probeHitDistanceDataWithBorderRes			= m_ddgiParams.probeHitDistanceDataRes + math::Vector2u::Constant(2u);

	m_ddgiParams.probesIlluminanceTextureRes				= math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(m_ddgiParams.probeIlluminanceDataWithBorderRes);
	m_ddgiParams.probesHitDistanceTextureRes				= math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(m_ddgiParams.probeHitDistanceDataWithBorderRes);

	m_ddgiParams.probesIlluminanceTexturePixelSize			= m_ddgiParams.probesIlluminanceTextureRes.cast<Real32>().cwiseInverse();
	m_ddgiParams.probesIlluminanceTextureUVDeltaPerProbe	= m_ddgiParams.probesIlluminanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeIlluminanceDataWithBorderRes.cast<Real32>());
	m_ddgiParams.probesIlluminanceTextureUVPerProbeNoBorder	= m_ddgiParams.probesIlluminanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeIlluminanceDataRes.cast<Real32>());
	
	m_ddgiParams.probesHitDistanceTexturePixelSize			= m_ddgiParams.probesHitDistanceTextureRes.cast<Real32>().cwiseInverse();
	m_ddgiParams.probesHitDistanceUVDeltaPerProbe			= m_ddgiParams.probesHitDistanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeHitDistanceDataWithBorderRes.cast<Real32>());
	m_ddgiParams.probesHitDistanceTextureUVPerProbeNoBorder	= m_ddgiParams.probesHitDistanceTexturePixelSize.cwiseProduct(m_ddgiParams.probeHitDistanceDataRes.cast<Real32>());
	
	m_ddgiParams.probeIlluminanceEncodingGamma				= 1.f;
}

void DDGISceneSubsystem::InitializeTextures()
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesTextureWidth		= m_ddgiParams.probesVolumeResolution.x() * m_ddgiParams.probesVolumeResolution.z();
	const Uint32 probesTextureHeight	= m_ddgiParams.probesVolumeResolution.y();

	const math::Vector2u illuminancePerProbeRes	= GetProbeIlluminanceWithBorderDataRes();
	const math::Vector2u distancePerProbeRes	= GetProbeDistancesDataWithBorderRes();

	const math::Vector3u probesRes = GetProbesVolumeResolution();

	const rhi::ETextureUsage texturesUsage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);

	rhi::TextureDefinition probesIlluminanceTextureDef;
	probesIlluminanceTextureDef.resolution	= math::Vector3u{ probesTextureWidth * illuminancePerProbeRes.x(), probesTextureHeight * illuminancePerProbeRes.y(), 1u };
	probesIlluminanceTextureDef.usage		= texturesUsage;
#if !SPT_RELEASE
	lib::AddFlag(probesIlluminanceTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE
	probesIlluminanceTextureDef.format		= rhi::EFragmentFormat::B10G11R11_U_Float;
	const lib::SharedRef<rdr::Texture> probesIlluminanceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Probes Illuminance"), probesIlluminanceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition probesIlluminanceViewDefinition;
	probesIlluminanceViewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_probesIlluminanceTextureView = probesIlluminanceTexture->CreateView(RENDERER_RESOURCE_NAME("Probes Illuminance View"), probesIlluminanceViewDefinition);

	rhi::TextureDefinition probesDistanceTextureDef;
	probesDistanceTextureDef.resolution	= math::Vector3u{ probesTextureWidth * distancePerProbeRes.x(), probesTextureHeight * distancePerProbeRes.y(), 1u };
	probesDistanceTextureDef.usage		= texturesUsage;
#if !SPT_RELEASE
	lib::AddFlag(probesDistanceTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE
	probesDistanceTextureDef.format		= rhi::EFragmentFormat::RG16_S_Float;
	const lib::SharedRef<rdr::Texture> probesDistanceTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Probes Distance"), probesDistanceTextureDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition probesDistanceViewDefinition;
	probesDistanceViewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	m_probesHitDistanceTextureView = probesDistanceTexture->CreateView(RENDERER_RESOURCE_NAME("Probes Distance View"), probesDistanceViewDefinition);
}

void DDGISceneSubsystem::OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	m_wantsGlobalUpdate = true;
}

} // spt::rsc
