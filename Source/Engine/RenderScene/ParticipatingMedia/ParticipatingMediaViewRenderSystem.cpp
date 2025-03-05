#include "ParticipatingMediaViewRenderSystem.h"
#include "RenderGraphBuilder.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "DDGI/DDGISceneSubsystem.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "GlobalResources/GlobalResources.h"
#include "RenderScene.h"
#include "ResourcesManager.h"
#include "Lights/ViewShadingInput.h"
#include "EngineFrame.h"
#include "Sequences.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"

namespace spt::rsc
{

namespace parameters
{

RendererFloatParameter constantDensity("Fog Density", { "Volumetric Fog" }, 0.035f, 0.f, 1.f);
RendererFloatParameter scatteringFactor("Scattering Factor", { "Volumetric Fog" }, 0.1f, 0.f, 1.f);
RendererFloatParameter phaseFunctionAnisotrophy("Phase Function Aniso", { "Volumetric Fog" }, 0.1f, 0.f, 1.f);

RendererFloatParameter fogFarPlane("Fog Far Plane", { "Volumetric Fog" }, 35.f, 1.f, 50.f);

RendererBoolParameter enableDirectionalLightsInScattering("Enable Directional Lights Scattering", { "Volumetric Fog" }, true);
RendererBoolParameter enableDirectionalLightsVolumetricRTShadows("Enable Directional Lights Volumetric RT Shadows", { "Volumetric Fog" }, true);

} // parameters

BEGIN_SHADER_STRUCT(VolumetricFogConstants)
    SHADER_STRUCT_FIELD(math::Vector2f, jitter2D)
    SHADER_STRUCT_FIELD(math::Vector2u, blueNoiseResMask)
    SHADER_STRUCT_FIELD(math::Vector3u, fogGridRes)
	SHADER_STRUCT_FIELD(Real32,	        fogNearPlane)
    SHADER_STRUCT_FIELD(math::Vector3f, fogGridInvRes)
	SHADER_STRUCT_FIELD(Real32,         fogFarPlane)
    SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(RenderVolumetricFogDS, rg::RGDescriptorSetState<RenderVolumetricFogDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VolumetricFogConstants>),                    u_fogConstants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                              u_blueNoiseTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_depthSampler)
DS_END();


struct VolumetricFogRenderingParams
{
	lib::MTHandle<RenderVolumetricFogDS> volumetricFogDS;
};


namespace tile_min_depth
{

BEGIN_SHADER_STRUCT(TileMinDepthConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, depthInvRes)
END_SHADER_STRUCT();


DS_BEGIN(RenderTileMinDepthDS, rg::RGDescriptorSetState<RenderTileMinDepthDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_minSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                       u_outMinDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TileMinDepthConstants>),                     u_constants)
DS_END();


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
	shaderConstants.depthInvRes = depthTexture->GetResolution2D().cast<Real32>().cwiseInverse();

	const lib::MTHandle<RenderTileMinDepthDS> tileMinDepthDS = graphBuilder.CreateDescriptorSet<RenderTileMinDepthDS>(RENDERER_RESOURCE_NAME("TileMinDepthDS"));
	tileMinDepthDS->u_depthTexture       = depthTexture;
	tileMinDepthDS->u_outMinDepthTexture = minDepthTexture;
	tileMinDepthDS->u_constants          = shaderConstants;

	static const rdr::PipelineStateID renderTileMinDepthPipeline = CompileRenderTileMinDepthPipeline();

	const math::Vector2u groupSize = math::Vector2u(16u, 8u); // 8x4 threads, each loads 2x2 quads

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Tile Min Depth"),
						  renderTileMinDepthPipeline,
						  math::Utils::DivideCeil(depthTexture->GetResolution2D(), groupSize),
						  rg::BindDescriptorSets(tileMinDepthDS));

	return minDepthTexture;
}

} // tile_min_depth


namespace participating_media
{

BEGIN_SHADER_STRUCT(RenderParticipatingMediaParams)
	SHADER_STRUCT_FIELD(math::Vector3f, constantFogColor)
	SHADER_STRUCT_FIELD(Real32, scatteringFactor)
	SHADER_STRUCT_FIELD(Real32, constantDensity)
	END_SHADER_STRUCT();


DS_BEGIN(RenderParticipatingMediaDS, rg::RGDescriptorSetState<RenderParticipatingMediaDS>)
DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>), u_participatingMediaTexture)
DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RenderParticipatingMediaParams>), u_participatingMediaParams)
DS_END();


static rdr::PipelineStateID CompileRenderParticipatingMediaPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/RenderParticipatingMedia.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ParticipatingMediaCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderParticipatingMediaPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	RenderParticipatingMediaParams pariticipatingMediaParams;
	pariticipatingMediaParams.constantFogColor = math::Vector3f::Constant(1.0f);
	pariticipatingMediaParams.scatteringFactor = parameters::scatteringFactor;
	pariticipatingMediaParams.constantDensity  = parameters::constantDensity;

	const lib::MTHandle<RenderParticipatingMediaDS> participatingMediaDS = graphBuilder.CreateDescriptorSet<RenderParticipatingMediaDS>(RENDERER_RESOURCE_NAME("RenderParticipatingMediaDS"));
	participatingMediaDS->u_participatingMediaTexture = fogParams.participatingMediaTextureView;
	participatingMediaDS->u_participatingMediaParams  = pariticipatingMediaParams;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID renderParticipatingMediaPipeline = CompileRenderParticipatingMediaPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Participating Media"),
						  renderParticipatingMediaPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(participatingMediaDS, fogRenderingParams.volumetricFogDS, renderView.GetRenderViewDS()));
}

} // participating_media

namespace in_scattering
{

namespace indirect
{

BEGIN_SHADER_STRUCT(IndirectInScatteringConstants)
	SHADER_STRUCT_FIELD(math::Vector3f, indirectGridRes)
END_SHADER_STRUCT();



DS_BEGIN(IndirectInScatteringDS, rg::RGDescriptorSetState<IndirectInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector3f>),                            u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_phaseFunctionLUTSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<IndirectInScatteringConstants>),          u_indirectInScatteringConstants)
DS_END();


static rdr::PipelineStateID CompileComputeIndirectInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeIndirectInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeIndirectInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("IndirectInScatteringPipeline"), shader);
}


static rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<ddgi::DDGISceneSubsystem> ddgiSubsystem = renderScene.GetSceneSubsystem<ddgi::DDGISceneSubsystem>();
	lib::MTHandle<ddgi::DDGISceneDS> ddgiDS = ddgiSubsystem ? ddgiSubsystem->GetDDGISceneDS() : nullptr;

	if (!ddgiDS.IsValid())
	{
		return rg::RGTextureViewHandle();
	}

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector3u indirectInScatteringRes = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(2u, 2u, 1u));

	rg::RGTextureViewHandle indirectInScatteringTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Indirect In Scattering Texture"),
																						 rg::TextureDef(indirectInScatteringRes, rhi::EFragmentFormat::B10G11R11_U_Float));

	IndirectInScatteringConstants indirectInScatteringConstants;
	indirectInScatteringConstants.indirectGridRes = indirectInScatteringRes.cast<Real32>();

	const lib::MTHandle<IndirectInScatteringDS> indirectInScatteringDS = graphBuilder.CreateDescriptorSet<IndirectInScatteringDS>(RENDERER_RESOURCE_NAME("IndirectInScatteringDS"));
	indirectInScatteringDS->u_inScatteringTexture           = indirectInScatteringTexture;
	indirectInScatteringDS->u_indirectInScatteringConstants = indirectInScatteringConstants;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(indirectInScatteringRes, math::Vector3u(4u, 4u, 2u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeIndirectInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute Indirect In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(indirectInScatteringDS,
												 fogRenderingParams.volumetricFogDS,
												 renderView.GetRenderViewDS(),
												 std::move(ddgiDS)));

	return indirectInScatteringTexture;
}

} // indirect

BEGIN_SHADER_STRUCT(VolumetricFogInScatteringParams)
	SHADER_STRUCT_FIELD(Real32,         paseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Bool,           hasValidHistory)
	SHADER_STRUCT_FIELD(Real32,         accumulationCurrentFrameWeight)
	SHADER_STRUCT_FIELD(Real32,         enableDirectionalLightsInScattering)
	SHADER_STRUCT_FIELD(Bool,           enableIndirectInScattering)
END_SHADER_STRUCT();


DS_BEGIN(ComputeInScatteringDS, rg::RGDescriptorSetState<ComputeInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),                            u_participatingMediaTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSample)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),                             u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector4f>),                    u_inScatteringHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector3f>),                    u_indirectInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VolumetricFogInScatteringParams>),         u_inScatteringParams)
DS_END();


static rdr::PipelineStateID CompileComputeInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("InScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ViewSpecShadingParameters& shadingParams = viewSpec.GetBlackboard().Get<ViewSpecShadingParameters>();

	const rg::RGTextureViewHandle indirectInScattering = indirect::Render(graphBuilder, renderScene, viewSpec, fogParams, fogRenderingParams);

	VolumetricFogInScatteringParams inScatteringParams;
	inScatteringParams.paseFunctionAnisotrophy             = parameters::phaseFunctionAnisotrophy;
	inScatteringParams.hasValidHistory                     = fogParams.inScatteringHistoryTextureView.IsValid();
	inScatteringParams.accumulationCurrentFrameWeight      = 0.05f;
	inScatteringParams.enableDirectionalLightsInScattering = parameters::enableDirectionalLightsInScattering;
	inScatteringParams.enableIndirectInScattering          = indirectInScattering.IsValid();

	const lib::MTHandle<ComputeInScatteringDS> computeInScatteringDS = graphBuilder.CreateDescriptorSet<ComputeInScatteringDS>(RENDERER_RESOURCE_NAME("ComputeInScatteringDS"));
	computeInScatteringDS->u_participatingMediaTexture   = fogParams.participatingMediaTextureView;
	computeInScatteringDS->u_inScatteringTexture         = fogParams.inScatteringTextureView;
	computeInScatteringDS->u_inScatteringHistoryTexture  = fogParams.inScatteringHistoryTextureView;
	computeInScatteringDS->u_inScatteringParams          = inScatteringParams;
	computeInScatteringDS->u_indirectInScatteringTexture = indirectInScattering;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(computeInScatteringDS,
												 fogRenderingParams.volumetricFogDS,
												 renderView.GetRenderViewDS(),
												 shadingParams.shadingInputDS,
												 shadingParams.shadowMapsDS));
}

} // in_scattering

namespace integrate_in_scattering
{


DS_BEGIN(IntegrateInScatteringDS, rg::RGDescriptorSetState<IntegrateInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),                            u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_inScatteringSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),                             u_integratedInScatteringTexture)
DS_END();


static rdr::PipelineStateID CompileIntegrateInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/IntegrateInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "IntegrateInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("IntegrateInScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams, const VolumetricFogRenderingParams& fogRenderingParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const lib::MTHandle<IntegrateInScatteringDS> computeInScatteringDS = graphBuilder.CreateDescriptorSet<IntegrateInScatteringDS>(RENDERER_RESOURCE_NAME("IntegrateInScatteringDS"));
	computeInScatteringDS->u_inScatteringTexture			= fogParams.inScatteringTextureView;
	computeInScatteringDS->u_integratedInScatteringTexture	= fogParams.integratedInScatteringTextureView;

	const math::Vector3u dispatchElements = math::Vector3u(fogParams.volumetricFogResolution.x(), fogParams.volumetricFogResolution.y(), 1u);
	const math::Vector3u dispatchSize = math::Utils::DivideCeil(dispatchElements, math::Vector3u(8u, 8u, 1u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileIntegrateInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Integrate In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(computeInScatteringDS, fogRenderingParams.volumetricFogDS, renderView.GetRenderViewDS()));
}

} // integrate_in_scattering


ParticipatingMediaViewRenderSystem::ParticipatingMediaViewRenderSystem()
{
}

void ParticipatingMediaViewRenderSystem::PreRenderFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::PreRenderFrame(graphBuilder, renderScene, viewSpec);

	if(viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
	{
		viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque).GetPostRenderStage().AddRawMember(this, &ParticipatingMediaViewRenderSystem::RenderParticipatingMedia);
	}
	else if (viewSpec.SupportsStage(ERenderStage::DeferredShading))
	{
		viewSpec.GetRenderStageEntries(ERenderStage::DeferredShading).GetPostRenderStage().AddRawMember(this, &ParticipatingMediaViewRenderSystem::RenderParticipatingMedia);
	}
}

const VolumetricFogParams& ParticipatingMediaViewRenderSystem::GetVolumetricFogParams() const
{
	return m_volumetricFogParams;
}

void ParticipatingMediaViewRenderSystem::RenderParticipatingMedia(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Participating Media");

	const rsc::RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingRes();

	const Uint32 volumetricTileSize = 8u;

	const math::Vector2u volumetricTileRes(volumetricTileSize, volumetricTileSize);
	const math::Vector2u volumetricTilesNum = math::Utils::DivideCeil(renderingRes, volumetricTileRes);

	const Uint32 volumetricFogZRes = 128u;

	const math::Vector3u volumetricFogRes(volumetricTilesNum.x(), volumetricTilesNum.y(), volumetricFogZRes);

	rg::TextureDef participatingMediaTextureDef;
	participatingMediaTextureDef.resolution = volumetricFogRes;
	participatingMediaTextureDef.format     = rhi::EFragmentFormat::RGBA16_UN_Float; // [Color (RGB), Extinction (A)]
	const rg::RGTextureViewHandle participatingMediaTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Participating Media Texture"), participatingMediaTextureDef);

	rg::TextureDef integratedInScatteringTextureDef;
	integratedInScatteringTextureDef.resolution = volumetricFogRes;
	integratedInScatteringTextureDef.format     = rhi::EFragmentFormat::RGBA16_S_Float; // [Integrated In-Scattering (RGB), Transmittance (A)]
	const rg::RGTextureViewHandle integratedInScatteringTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Integrated In-Scattering Texture"), integratedInScatteringTextureDef);

	PrepareViewTextures(volumetricFogRes);
	
	SPT_CHECK(!!m_currentInScatteringTexture);
	const rg::RGTextureViewHandle inScatteringTextureView = graphBuilder.AcquireExternalTextureView(m_currentInScatteringTexture);
	
	rg::RGTextureViewHandle inScatteringHistoryTextureView;
	if (m_prevFrameInScatteringTexture)
	{
		inScatteringHistoryTextureView = graphBuilder.AcquireExternalTextureView(m_prevFrameInScatteringTexture);
	}

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
	fogConstants.fogNearPlane      = renderView.GetNearPlane();
	fogConstants.fogFarPlane       = parameters::fogFarPlane;
	fogConstants.frameIdx          = renderView.GetRenderedFrameIdx();
	fogConstants.jitter2D          = jitter;
	fogConstants.blueNoiseResMask  = blueNoiseResolutionMask;
	fogConstants.fogGridRes        = volumetricFogRes;
	fogConstants.fogGridInvRes     = volumetricFogRes.cast<Real32>().cwiseInverse();

	const ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	const rg::RGTextureViewHandle tileMinDepth = tile_min_depth::RenderTileMinDepth(graphBuilder, shadingViewContext.depth, math::Vector2u(volumetricTileSize, volumetricTileSize));

	VolumetricFogRenderingParams fogRenderingParams;
	fogRenderingParams.volumetricFogDS = graphBuilder.CreateDescriptorSet<RenderVolumetricFogDS>(RENDERER_RESOURCE_NAME("VolumetricFogDS"));
	fogRenderingParams.volumetricFogDS->u_fogConstants     = fogConstants;
	fogRenderingParams.volumetricFogDS->u_blueNoiseTexture = blueNoiseTexture;
	fogRenderingParams.volumetricFogDS->u_depthTexture     = tileMinDepth;

	m_volumetricFogParams = VolumetricFogParams{};
	m_volumetricFogParams.participatingMediaTextureView     = participatingMediaTextureView;
	m_volumetricFogParams.inScatteringTextureView           = inScatteringTextureView;
	m_volumetricFogParams.integratedInScatteringTextureView = integratedInScatteringTextureView;
	m_volumetricFogParams.inScatteringHistoryTextureView    = inScatteringHistoryTextureView;
	m_volumetricFogParams.volumetricFogResolution           = volumetricFogRes;
	m_volumetricFogParams.nearPlane                         = renderView.GetNearPlane();
	m_volumetricFogParams.farPlane                          = parameters::fogFarPlane;

	participating_media::Render(graphBuilder, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	in_scattering::Render(graphBuilder, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	integrate_in_scattering::Render(graphBuilder, renderScene, viewSpec, m_volumetricFogParams, fogRenderingParams);

	std::swap(m_currentInScatteringTexture, m_prevFrameInScatteringTexture);
}

void ParticipatingMediaViewRenderSystem::PrepareViewTextures(const math::Vector3u& volumetricFogResolution)
{
	if (!m_currentInScatteringTexture || m_currentInScatteringTexture->GetResolution() != volumetricFogResolution)
	{
		rhi::TextureDefinition inScatteringTextureDef;
		inScatteringTextureDef.resolution	= volumetricFogResolution;
		inScatteringTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
#if SPT_DEBUG || SPT_DEVELOPMENT
		lib::AddFlag(inScatteringTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif
		inScatteringTextureDef.format = rhi::EFragmentFormat::RGBA16_S_Float; // [In-Scattering (RGB), Extinction (A)]
		m_currentInScatteringTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("In-Scattering Texture"), inScatteringTextureDef, rhi::EMemoryUsage::GPUOnly);
	}
}

} // spt::rsc
