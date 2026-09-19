#include "ParticipatingMediaViewRenderSystem.h"
#include "RenderGraphBuilder.h"
#include "Utils/SceneRenderingTypes.h"
#include "Utils/ViewRenderingSpec.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "SceneRenderSystems/DDGI/DDGIRenderSystem.h"
#include "ShaderStructs/ShaderStructs.h"
#include "GlobalResources/GlobalResources.h"
#include "RenderScene.h"
#include "ResourcesManager.h"
#include "SceneRenderSystems/Lights/ViewShadingInput.h"
#include "EngineFrame.h"
#include "Sequences.h"
#include "SceneRenderer/Utils/GaussianBlurRenderer.h"
#include "SceneRenderSystems/Atmosphere/Clouds/VolumetricCloudsTypes.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereRenderSystem.h"


namespace spt::rsc
{

SPT_REGISTER_VIEW_RENDER_SYSTEM(ParticipatingMediaViewRenderSystem)

namespace parameters
{

RendererFloatParameter constantFogDensity("Constant Fog Density", { "Volumetric Fog" }, 0.005f, 0.f, 1.f);
RendererFloatParameter constantFogExtinction("Constant Fog Extinction", { "Volumetric Fog" }, 0.3f, 0.f, 10.f);
RendererFloat3Parameter consantFogAlbedo("Constant Fox Albedo", { "Volumetric Fog" }, math::Vector3f::Constant(1.f), 0.f, 1.f);

RendererFloatParameter fogHeightFalloff("Fog Height Falloff", { "Volumetric Fog" }, 0.0042f, 0.f, 10.f);
RendererFloatParameter fogHeightAbsorptionPercentage("Fog Height Absorption %", { "Volumetric Fog" }, 0.2f, 0.f, 1.f);

RendererFloatParameter phaseFunctionAnisotrophy("Phase Function Aniso", { "Volumetric Fog" }, 0.84f, 0.f, 1.f);

RendererFloatParameter fogFarPlane("Fog Far Plane", { "Volumetric Fog" }, 8000.f, 1.f, 40000.f);

RendererBoolParameter enableDirectionalLightsInScattering("Enable Directional Lights Scattering", { "Volumetric Fog" }, true);
RendererBoolParameter enableDirectionalLightsVolumetricRTShadows("Enable Directional Lights Volumetric RT Shadows", { "Volumetric Fog" }, true);

RendererBoolParameter enableVolumetricFogBlur("Enable Volumetric Fog Blur", { "Volumetric Fog" }, true);
RendererFloatParameter volumetricFogBlurSigma("Volumetric Fog Blur Sigma", { "Volumetric Fog" }, 1.4f, 0.f, 10.f);
RendererIntParameter volumetricFogBlurKernelSize("Volumetric Fog Blur Kernel Size", { "Volumetric Fog" }, 2, 1, 10);
RendererBoolParameter fogBlurEnableTonemap("Fog Blur Enable Tonemap", { "Volumetric Fog" }, true);

} // parameters

BEGIN_SHADER_STRUCT(VolumetricFogConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                    blueNoiseResMask)
	SHADER_STRUCT_FIELD(math::Vector3u,                    fogGridRes)
	SHADER_STRUCT_FIELD(Real32,                            fogNearPlane)
	SHADER_STRUCT_FIELD(math::Vector3f,                    fogGridInvRes)
	SHADER_STRUCT_FIELD(Real32,                            fogFarPlane)
	SHADER_STRUCT_FIELD(Uint32,                            localLightsScatteringMaxDepth)
	SHADER_STRUCT_FIELD(Real32,                            localLightsScatteringWNormalization)
	SHADER_STRUCT_FIELD(Uint32,                            frameIdx)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, blueNoiseTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
END_SHADER_STRUCT();


struct VolumetricFogRenderingParams
{
	rdr::GPUPtr<VolumetricFogConstants> volumetricFogData;
};


namespace tile_min_depth
{

BEGIN_SHADER_STRUCT(TileMinDepthConstants)
	SHADER_STRUCT_FIELD(math::Vector2f,            depthInvRes)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, depthTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, outMinDepthTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileRenderTileMinDepthPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/RenderTileMinDepth.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TileMinDepthCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderTileMinDepthPipeline"), shader);
}


rg::RGTextureViewHandle RenderTileMinDepth(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle depthTexture, math::Vector2u tileSize)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(tileSize == math::Vector2u(8u, 8u)); // optimized only for 8x8 tiles

	const math::Vector2u tilesNum = math::Utils::DivideCeil(depthTexture->GetResolution2D(), tileSize);

	const rg::RGTextureViewHandle minDepthTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Min Depth Texture"),
																				   rg::TextureDef(tilesNum, rhi::EFragmentFormat::R32_S_Float));

	TileMinDepthConstants shaderConstants;
	shaderConstants.depthInvRes        = depthTexture->GetResolution2D().cast<Real32>().cwiseInverse();
	shaderConstants.depthTexture       = depthTexture;
	shaderConstants.outMinDepthTexture = minDepthTexture;

	static const rdr::PipelineStateID renderTileMinDepthPipeline = CompileRenderTileMinDepthPipeline();

	const math::Vector2u groupSize = math::Vector2u(16u, 8u); // 8x4 threads, each loads 2x2 quads

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Tile Min Depth"),
						  renderTileMinDepthPipeline,
						  math::Utils::DivideCeil(depthTexture->GetResolution2D(), groupSize),
						  rg::ShaderParams(shaderConstants));

	return minDepthTexture;
}

} // tile_min_depth


namespace participating_media
{

BEGIN_SHADER_STRUCT(RenderParticipatingMediaParams)
	SHADER_STRUCT_FIELD(math::Vector3f,                    constantFogAlbedo)
	SHADER_STRUCT_FIELD(Real32,                            constantFogDensity)
	SHADER_STRUCT_FIELD(Real32,                            constantFogExtinction)
	SHADER_STRUCT_FIELD(Real32,                            fogHeightFalloff)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>, participatingMediaTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileRenderParticipatingMediaPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/RenderParticipatingMedia.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ParticipatingMediaCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderParticipatingMediaPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	RenderParticipatingMediaParams pariticipatingMediaParams;
	pariticipatingMediaParams.constantFogAlbedo         = parameters::consantFogAlbedo;
	pariticipatingMediaParams.constantFogDensity        = parameters::constantFogDensity;
	pariticipatingMediaParams.constantFogExtinction     = parameters::constantFogExtinction;
	pariticipatingMediaParams.fogHeightFalloff          = parameters::fogHeightFalloff;
	pariticipatingMediaParams.participatingMediaTexture = fogParams.participatingMediaTextureView;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID renderParticipatingMediaPipeline = CompileRenderParticipatingMediaPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Participating Media"),
						  renderParticipatingMediaPipeline,
						  dispatchSize,
						  rg::ShaderParams(pariticipatingMediaParams, fogRenderingParams.volumetricFogData));
}

} // participating_media

namespace shadow_term
{

BEGIN_SHADER_STRUCT(VolumetricFogShadowTermConstants)
	SHADER_STRUCT_FIELD(math::Matrix4f,            cloudsTransmittanceMapViewProj)
	SHADER_STRUCT_FIELD(Bool,                      hasCloudsTransmittanceMap)
	SHADER_STRUCT_FIELD(Bool,                      hasValidHistory)
	SHADER_STRUCT_FIELD(Real32,                    accumulationCurrentFrameWeight)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<Real32>, rwDirLightShadowTerm)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<Real32>, historyDirLightShadowTerm)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, cloudsTransmittanceMap)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileComputeDirectionalLightShadowTermPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeDirectionalLightShadowTerm.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeDirectionalLightShadowTermCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DirectionalLightShadowTermPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const AtmosphereRenderSystem& atmosphereRenderSystem = rendererInterface.GetRenderSystemChecked<AtmosphereRenderSystem>();

	const ViewSpecShadingParameters& shadingParams = viewSpec.GetBlackboard().Get<ViewSpecShadingParameters>();

	const clouds::CloudsTransmittanceMap* transmittanceMap = atmosphereRenderSystem.AreVolumetricCloudsEnabled() ? &atmosphereRenderSystem.GetCloudsTransmittanceMap() : nullptr;

	VolumetricFogShadowTermConstants shadowTermConstants;
	shadowTermConstants.hasValidHistory                = fogParams.historyDirectionalLightShadowTerm.IsValid();
	shadowTermConstants.accumulationCurrentFrameWeight = 0.2f;

	if (transmittanceMap)
	{
		shadowTermConstants.cloudsTransmittanceMapViewProj = transmittanceMap->viewProjectionMatrix;
		shadowTermConstants.hasCloudsTransmittanceMap      = true;
	}

	shadowTermConstants.rwDirLightShadowTerm      = fogParams.directionalLightShadowTerm;
	shadowTermConstants.historyDirLightShadowTerm = fogParams.historyDirectionalLightShadowTerm;

	if (transmittanceMap)
	{
		shadowTermConstants.cloudsTransmittanceMap = transmittanceMap->cloudsTransmittanceTexture;
	}

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID pipline = CompileComputeDirectionalLightShadowTermPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute Directional Light Shadow Term"),
						  pipline,
						  dispatchSize,
						  rg::ShaderParams(shadowTermConstants, fogRenderingParams.volumetricFogData, shadingParams.viewShadingParams));
}

} // shadow_term

namespace in_scattering
{

namespace indirect
{

BEGIN_SHADER_STRUCT(IndirectInScatteringConstants)
	SHADER_STRUCT_FIELD(math::Vector3f,                    indirectGridRes)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector3f>, inScatteringTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileComputeIndirectInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeIndirectInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeIndirectInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("IndirectInScatteringPipeline"), shader);
}


static Bool Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const ddgi::DDGIRenderSystem* ddgiRenderSystem = rendererInterface.GetRenderSystem<ddgi::DDGIRenderSystem>();
	const rdr::GPUPtr<ddgi::DDGIGPUScene>& ddgiGPUScene = ddgiRenderSystem ? ddgiRenderSystem->GetDDGIGPUScene() : nullptr;

	if (!ddgiGPUScene.IsValid())
	{
		graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Indirect In-Scattering Texture"), fogParams.indirectInScatteringTextureView, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
		return false;
	}

	const math::Vector3u indirectInScatteringRes = fogParams.indirectInScatteringTextureView->GetResolution();

	IndirectInScatteringConstants indirectInScatteringConstants;
	indirectInScatteringConstants.indirectGridRes     = indirectInScatteringRes.cast<Real32>();
	indirectInScatteringConstants.inScatteringTexture = fogParams.indirectInScatteringTextureView;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(indirectInScatteringRes, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeIndirectInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute Indirect In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::ShaderParams(indirectInScatteringConstants,
												 fogRenderingParams.volumetricFogData,
												 ddgiGPUScene));

	return true;
}

} // indirect

BEGIN_SHADER_STRUCT(VolumetricFogInScatteringParams)
	SHADER_STRUCT_FIELD(Real32,                            paseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Real32,                            enableDirectionalLightsInScattering)
	SHADER_STRUCT_FIELD(Bool,                              enableIndirectInScattering)
	SHADER_STRUCT_FIELD(Bool,                              hasValidHistory)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, participatingMediaTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<Real32>,         directionalLightShadowTerm)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>, inScatteringTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector3f>, indirectInScatteringTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>, localLightsInScatteringTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, historyLocalLightsInScatteringTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileComputeInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("InScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const ViewSpecShadingParameters& shadingParams = viewSpec.GetBlackboard().Get<ViewSpecShadingParameters>();

	const Bool hasValidIndirectInScattering = indirect::Render(graphBuilder, rendererInterface, renderScene, viewSpec, fogParams, fogRenderingParams);

	VolumetricFogInScatteringParams inScatteringParams;
	inScatteringParams.paseFunctionAnisotrophy               = parameters::phaseFunctionAnisotrophy;
	inScatteringParams.enableDirectionalLightsInScattering   = parameters::enableDirectionalLightsInScattering;
	inScatteringParams.enableIndirectInScattering            = hasValidIndirectInScattering;
	inScatteringParams.hasValidHistory                       = fogParams.historyLocalLightsInScattering.IsValid();
	inScatteringParams.participatingMediaTexture             = fogParams.participatingMediaTextureView;
	inScatteringParams.directionalLightShadowTerm            = fogParams.directionalLightShadowTerm;
	inScatteringParams.inScatteringTexture                   = fogParams.inScatteringTextureView;
	inScatteringParams.localLightsInScatteringTexture        = fogParams.localLightsInScattering;
	inScatteringParams.historyLocalLightsInScatteringTexture = fogParams.historyLocalLightsInScattering;
	inScatteringParams.indirectInScatteringTexture           = fogParams.indirectInScatteringTextureView;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::ShaderParams(inScatteringParams, fogRenderingParams.volumetricFogData, shadingParams.viewShadingParams));
}

} // in_scattering

namespace integrate_in_scattering
{

BEGIN_SHADER_STRUCT(IntegrateInScatteringConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, inScatteringTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>, integratedInScatteringTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileIntegrateInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/IntegrateInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "IntegrateInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("IntegrateInScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	IntegrateInScatteringConstants integrateInScatteringParams;
	integrateInScatteringParams.inScatteringTexture           = fogParams.inScatteringTextureView;
	integrateInScatteringParams.integratedInScatteringTexture = fogParams.integratedInScatteringTextureView;

	const math::Vector3u dispatchElements = math::Vector3u(fogParams.volumetricFogResolution.x(), fogParams.volumetricFogResolution.y(), 1u);
	const math::Vector3u dispatchSize = math::Utils::DivideCeil(dispatchElements, math::Vector3u(8u, 8u, 1u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileIntegrateInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Integrate In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::ShaderParams(integrateInScatteringParams, fogRenderingParams.volumetricFogData));
}

} // integrate_in_scattering


ParticipatingMediaViewRenderSystem::ParticipatingMediaViewRenderSystem(RenderView& inRenderView)
	: Super(inRenderView)
	, m_directionalLightShadowTerm(RENDERER_RESOURCE_NAME("Directional Light Shadow Term"))
	, m_localLightsInScattering(RENDERER_RESOURCE_NAME("Local Lights In Scattering"))
{
	rhi::TextureDefinition shadowTermDef;
	shadowTermDef.format = rhi::EFragmentFormat::R16_UN_Float;
	shadowTermDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
	m_directionalLightShadowTerm.SetDefinition(shadowTermDef);

	rhi::TextureDefinition localLightsInScatteringDef;
	localLightsInScatteringDef.format = rhi::EFragmentFormat::RGBA16_S_Float;
	localLightsInScatteringDef.usage  = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
	m_localLightsInScattering.SetDefinition(localLightsInScatteringDef);
}

void ParticipatingMediaViewRenderSystem::BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::BeginFrame(graphBuilder, renderScene, viewSpec);

	viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderParticipatingMedia).AddRawMember(this, &ParticipatingMediaViewRenderSystem::RenderParticipatingMedia);
}

const VolumetricFogParams& ParticipatingMediaViewRenderSystem::GetVolumetricFogParams() const
{
	return m_volumetricFogParams;
}

HeightFogParams ParticipatingMediaViewRenderSystem::GetHeightFogParams() const
{
	HeightFogParams heightFogParams;
	heightFogParams.density              = parameters::constantFogDensity;
	heightFogParams.extinction           = parameters::constantFogExtinction;
	heightFogParams.albedo               = parameters::consantFogAlbedo;
	heightFogParams.heightFalloff        = parameters::fogHeightFalloff;
	heightFogParams.absorptionPercentage = parameters::fogHeightAbsorptionPercentage;
	return heightFogParams;
}

void ParticipatingMediaViewRenderSystem::RenderParticipatingMedia(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Participating Media");

	const rsc::RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = viewSpec.GetRenderingRes();

	const Uint32 volumetricTileSize = 8u;

	const math::Vector2u volumetricTileRes(volumetricTileSize, volumetricTileSize);
	const math::Vector2u volumetricTilesNum = math::Utils::DivideCeil(renderingRes, volumetricTileRes);

	const Uint32 volumetricFogZRes = 128u;
	const Uint32 volumetricFogLocalLightsZRes = 80u;

	const math::Vector3u volumetricFogRes(volumetricTilesNum.x(), volumetricTilesNum.y(), volumetricFogZRes);
	const math::Vector3u localInScatteringRes(volumetricTilesNum.x(), volumetricTilesNum.y(), volumetricFogLocalLightsZRes);

	m_directionalLightShadowTerm.Update(volumetricFogRes);
	m_localLightsInScattering.Update(localInScatteringRes);

	rg::TextureDef participatingMediaTextureDef;
	participatingMediaTextureDef.resolution = volumetricFogRes;
	participatingMediaTextureDef.format     = rhi::EFragmentFormat::RGBA16_UN_Float; // [Color (RGB), Extinction (A)]
	const rg::RGTextureViewHandle participatingMediaTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Participating Media Texture"), participatingMediaTextureDef);

	rg::TextureDef integratedInScatteringTextureDef;
	integratedInScatteringTextureDef.resolution = volumetricFogRes;
	integratedInScatteringTextureDef.format     = rhi::EFragmentFormat::RGBA16_S_Float; // [Integrated In-Scattering (RGB), Transmittance (A)]
	const rg::RGTextureViewHandle integratedInScatteringTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Integrated In-Scattering Texture"), integratedInScatteringTextureDef);

	const rg::RGTextureViewHandle inScatteringTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("In-Scattering Texture"), rg::TextureDef(volumetricFogRes, rhi::EFragmentFormat::RGBA16_S_Float));


	const math::Vector3u indirectInScatteringRes = math::Utils::DivideCeil(volumetricFogRes, math::Vector3u(2u, 2u, 1u));

	rg::RGTextureViewHandle indirectInScatteringTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Indirect In Scattering Texture"),
																						 rg::TextureDef(indirectInScatteringRes, rhi::EFragmentFormat::B10G11R11_U_Float));

	const Uint64 frameIdx = renderScene.GetCurrentFrameRef().GetFrameIdx();
	const Uint32 jitterSequenceIdx = static_cast<Uint32>(frameIdx) & 7u;

	const math::Vector2f jitterSequence = math::Vector2f(math::Sequences::Halton<Real32>(jitterSequenceIdx, 2),
														 math::Sequences::Halton<Real32>(jitterSequenceIdx, 3));

	const math::Vector2f jitterValue = (jitterSequence - math::Vector2f::Constant(0.5f)).cwiseProduct(math::Vector2f::Constant(0.5f));
	const math::Vector2f jitter = jitterValue.cwiseProduct(volumetricFogRes.head<2>().cast<Real32>().cwiseInverse());

	const rg::RGTextureViewHandle blueNoiseTexture = graphBuilder.AcquireExternalTextureView(gfx::global::Resources::Get().blueNoise256.GetView());
	SPT_CHECK(!!blueNoiseTexture);
	SPT_CHECK(math::Utils::IsPowerOf2(blueNoiseTexture->GetResolution().x()));
	SPT_CHECK(math::Utils::IsPowerOf2(blueNoiseTexture->GetResolution().y()));

	const math::Vector2u blueNoiseResolutionMask = blueNoiseTexture->GetResolution2D() - math::Vector2u::Ones();

	VolumetricFogConstants fogConstants;
	fogConstants.fogNearPlane                        = renderView.GetNearPlane();
	fogConstants.fogFarPlane                         = parameters::fogFarPlane;
	fogConstants.frameIdx                            = viewSpec.GetFrameIdx();
	fogConstants.blueNoiseResMask                    = blueNoiseResolutionMask;
	fogConstants.fogGridRes                          = volumetricFogRes;
	fogConstants.fogGridInvRes                       = volumetricFogRes.cast<Real32>().cwiseInverse();
	fogConstants.localLightsScatteringMaxDepth       = volumetricFogLocalLightsZRes;
	fogConstants.localLightsScatteringWNormalization = volumetricFogZRes / static_cast<Real32>(volumetricFogLocalLightsZRes);

	const rg::RGTextureViewHandle dirLightShadowTerm = graphBuilder.AcquireExternalTextureView(m_directionalLightShadowTerm.GetCurrent());

	VolumetricFogRenderingParams fogRenderingParams;
	fogRenderingParams.volumetricFogData = graphBuilder.CreateGPUData(fogConstants);

	m_volumetricFogParams = VolumetricFogParams{};
	m_volumetricFogParams.participatingMediaTextureView     = participatingMediaTextureView;
	m_volumetricFogParams.inScatteringTextureView           = inScatteringTextureView;
	m_volumetricFogParams.indirectInScatteringTextureView   = indirectInScatteringTexture;
	m_volumetricFogParams.integratedInScatteringTextureView = integratedInScatteringTextureView;
	m_volumetricFogParams.volumetricFogResolution           = volumetricFogRes;
	m_volumetricFogParams.directionalLightShadowTerm        = dirLightShadowTerm;
	m_volumetricFogParams.historyDirectionalLightShadowTerm = graphBuilder.TryAcquireExternalTextureView(m_directionalLightShadowTerm.GetHistory());
	m_volumetricFogParams.localLightsInScattering           = graphBuilder.AcquireExternalTextureView(m_localLightsInScattering.GetCurrent());
	m_volumetricFogParams.historyLocalLightsInScattering    = graphBuilder.TryAcquireExternalTextureView(m_localLightsInScattering.GetHistory());
	m_volumetricFogParams.nearPlane                         = renderView.GetNearPlane();
	m_volumetricFogParams.farPlane                          = parameters::fogFarPlane;

	participating_media::Render(graphBuilder, rendererInterface, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	shadow_term::Render(graphBuilder, rendererInterface, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	in_scattering::Render(graphBuilder, rendererInterface, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	integrate_in_scattering::Render(graphBuilder, rendererInterface, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	{
		RenderViewEntryDelegates::RenderAerialPerspectiveData apData;
		apData.fogParams = &m_volumetricFogParams;

		RenderViewEntryContext apContext;
		apContext.Bind(apData);
		viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderAerialPerspective).Broadcast(graphBuilder, rendererInterface, renderScene, viewSpec, apContext);
	}

	if (parameters::enableVolumetricFogBlur)
	{
		gaussian_blur_renderer::GaussianBlur2DParams gaussianBlurParams;
		gaussianBlurParams.horizontalPass      = gaussian_blur_renderer::BlurPassParams{ static_cast<Uint32>(parameters::volumetricFogBlurKernelSize), parameters::volumetricFogBlurSigma };
		gaussianBlurParams.verticalPass        = gaussian_blur_renderer::BlurPassParams{ static_cast<Uint32>(parameters::volumetricFogBlurKernelSize), parameters::volumetricFogBlurSigma };
		gaussianBlurParams.useTonemappedValues = parameters::fogBlurEnableTonemap;

		m_volumetricFogParams.integratedInScatteringTextureView = gaussian_blur_renderer::ApplyGaussianBlur2D(graphBuilder,
																											  RG_DEBUG_NAME("Integrated Fog Blur"),
																											  m_volumetricFogParams.integratedInScatteringTextureView,
																											  gaussianBlurParams);
	}
}

} // spt::rsc
