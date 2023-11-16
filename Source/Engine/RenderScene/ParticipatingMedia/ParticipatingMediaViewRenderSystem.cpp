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
#include "RenderScene.h"
#include "ResourcesManager.h"
#include "SceneRenderer/Utils/DDGIScatteringPhaseFunctionLUT.h"
#include "Lights/ViewShadingInput.h"
#include "EngineFrame.h"
#include "Sequences.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"


namespace spt::rsc
{

namespace parameters
{

RendererFloatParameter fogMinDensity("Fog Min Density", { "Volumetric Fog" }, 0.09f, 0.f, 1.f);
RendererFloatParameter fogMaxDensity("Fog Max Density", { "Volumetric Fog" }, 0.6f, 0.f, 1.f);
RendererFloatParameter scatteringFactor("Scattering Factor", { "Volumetric Fog" }, 0.1f, 0.f, 1.f);
RendererFloatParameter localLightsPhaseFunctionAnisotrophy("Local Lights Phase Function Aniso", { "Volumetric Fog" }, 0.4f, 0.f, 1.f);
RendererFloatParameter dirLightsPhaseFunctionAnisotrophy("Directional Lights Phase Function Aniso", { "Volumetric Fog" }, 0.2f, 0.f, 1.f);

RendererFloatParameter fogFarPlane("Fog Far Plane", { "Volumetric Fog" }, 20.f, 1.f, 50.f);

RendererBoolParameter enableDirectionalLightsInScattering("Enable Directional Lights Scattering", { "Volumetric Fog" }, true);
RendererBoolParameter enableDirectionalLightsVolumetricRTShadows("Enable Directional Lights Volumetric RT Shadows", { "Volumetric Fog" }, true);

} // parameters

namespace participating_media
{

BEGIN_SHADER_STRUCT(RenderParticipatingMediaParams)
	SHADER_STRUCT_FIELD(math::Vector3f, constantFogColor)
	SHADER_STRUCT_FIELD(Real32, scatteringFactor)
	SHADER_STRUCT_FIELD(Real32,	fogNearPlane)
	SHADER_STRUCT_FIELD(Real32, fogFarPlane)
    SHADER_STRUCT_FIELD(Real32, densityNoiseThreshold)
    SHADER_STRUCT_FIELD(Real32, densityNoiseZSigma)
    SHADER_STRUCT_FIELD(math::Vector3f, densityNoiseSpeed)
    SHADER_STRUCT_FIELD(Real32, maxDensity)
    SHADER_STRUCT_FIELD(Real32, minDensity)
END_SHADER_STRUCT();


DS_BEGIN(RenderParticipatingMediaDS, rg::RGDescriptorSetState<RenderParticipatingMediaDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),								u_participatingMediaTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<RenderParticipatingMediaParams>),	u_participatingMediaParams)
DS_END();


static rdr::PipelineStateID CompileRenderParticipatingMediaPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/RenderParticipatingMedia.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ParticipatingMediaCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderParticipatingMediaPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	RenderParticipatingMediaParams pariticipatingMediaParams;
	pariticipatingMediaParams.constantFogColor		= math::Vector3f::Constant(1.0f);
	pariticipatingMediaParams.scatteringFactor		= parameters::scatteringFactor;
	pariticipatingMediaParams.fogNearPlane			= fogParams.nearPlane;
	pariticipatingMediaParams.fogFarPlane			= fogParams.farPlane;
    pariticipatingMediaParams.maxDensity			= parameters::fogMaxDensity;
    pariticipatingMediaParams.minDensity			= parameters::fogMinDensity;
	pariticipatingMediaParams.densityNoiseThreshold = 0.45f;
    pariticipatingMediaParams.densityNoiseZSigma	= -6.f;
    pariticipatingMediaParams.densityNoiseSpeed		= math::Vector3f(-0.5f, -0.5f, 0.1f);

	const lib::MTHandle<RenderParticipatingMediaDS> participatingMediaDS = graphBuilder.CreateDescriptorSet<RenderParticipatingMediaDS>(RENDERER_RESOURCE_NAME("RenderParticipatingMediaDS"));
	participatingMediaDS->u_participatingMediaTexture	= fogParams.participatingMediaTextureView;
	participatingMediaDS->u_participatingMediaParams	= pariticipatingMediaParams;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID renderParticipatingMediaPipeline = CompileRenderParticipatingMediaPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Participating Media"),
						  renderParticipatingMediaPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(participatingMediaDS, renderView.GetRenderViewDS()));
}

} // participating_media

namespace in_scattering
{

namespace indirect
{

BEGIN_SHADER_STRUCT(IndirectInScatteringParams)
	SHADER_STRUCT_FIELD(math::Vector3f,	jitter)
	SHADER_STRUCT_FIELD(Real32,			phaseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Real32,			fogNearPlane)
	SHADER_STRUCT_FIELD(Real32,			fogFarPlane)
	SHADER_STRUCT_FIELD(math::Matrix4f,	randomRotation)
END_SHADER_STRUCT();


DS_BEGIN(IndirectInScatteringDS, rg::RGDescriptorSetState<IndirectInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector3f>),                               u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<IndirectInScatteringParams>),       u_inScatteringParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_ddgiScatteringPhaseFunctionLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),    u_phaseFunctionLUTSampler)
DS_END();


static rdr::PipelineStateID CompileComputeIndirectInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeIndirectInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeIndirectInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("IndirectInScatteringPipeline"), shader);
}


static rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<DDGISceneSubsystem> ddgiSubsystem = renderScene.GetSceneSubsystem<DDGISceneSubsystem>();
	lib::MTHandle<DDGIDS> ddgiDS = ddgiSubsystem ? ddgiSubsystem->GetDDGIDS() : nullptr;

	if (!ddgiDS.IsValid())
	{
		return rg::RGTextureViewHandle();
	}

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector3u indirectInScatteringRes = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(2u, 2u, 2u));

	rg::RGTextureViewHandle indirectInScatteringTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Indirect In Scattering Texture"),
																							   rg::TextureDef(indirectInScatteringRes, rhi::EFragmentFormat::B10G11R11_U_Float));

	IndirectInScatteringParams inScatteringParams;
	inScatteringParams.jitter                                       = fogParams.fogJitter;
	inScatteringParams.phaseFunctionAnisotrophy                     = parameters::localLightsPhaseFunctionAnisotrophy;
	inScatteringParams.fogNearPlane                                 = fogParams.nearPlane;
	inScatteringParams.fogFarPlane                                  = fogParams.farPlane;
	inScatteringParams.randomRotation                               = math::Matrix4f::Identity();
	inScatteringParams.randomRotation.topLeftCorner<3, 3>()         = lib::rnd::RandomRotationMatrix();

	const rg::RGTextureViewHandle ddgiScatteringPhaseFunctionLUT = DDGIScatteringPhaseFunctionLUT::Get().GetLUT(graphBuilder);

	const lib::MTHandle<IndirectInScatteringDS> indirectInScatteringDS = graphBuilder.CreateDescriptorSet<IndirectInScatteringDS>(RENDERER_RESOURCE_NAME("IndirectInScatteringDS"));
	indirectInScatteringDS->u_inScatteringTexture            = indirectInScatteringTexture;
	indirectInScatteringDS->u_inScatteringParams             = inScatteringParams;
	indirectInScatteringDS->u_ddgiScatteringPhaseFunctionLUT = ddgiScatteringPhaseFunctionLUT;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(indirectInScatteringRes, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeIndirectInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute Indirect In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(indirectInScatteringDS,
												 renderView.GetRenderViewDS(),
												 std::move(ddgiDS)));

	return indirectInScatteringTexture;
}



} // indirect

BEGIN_SHADER_STRUCT(VolumetricFogInScatteringParams)
	SHADER_STRUCT_FIELD(math::Vector3f, jitter)
	SHADER_STRUCT_FIELD(Real32,         localLightsPhaseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Real32,         dirLightsPhaseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Real32,         fogNearPlane)
	SHADER_STRUCT_FIELD(Real32,         fogFarPlane)
	SHADER_STRUCT_FIELD(Bool,           hasValidHistory)
	SHADER_STRUCT_FIELD(Real32,         accumulationCurrentFrameWeight)
	SHADER_STRUCT_FIELD(Real32,         enableDirectionalLightsInScattering)
	SHADER_STRUCT_FIELD(Bool,           enableIndirectInScattering)
END_SHADER_STRUCT();


DS_BEGIN(ComputeInScatteringDS, rg::RGDescriptorSetState<ComputeInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),								u_participatingMediaTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_participatingMediaSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),								u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector4f>),						u_inScatteringHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_inScatteringHistorySampler)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector3f>),						u_indirectInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_indirectInScatteringSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<VolumetricFogInScatteringParams>),	u_inScatteringParams)
DS_END();


static rdr::PipelineStateID CompileComputeInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("InScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ViewSpecShadingParameters& shadingParams = viewSpec.GetData().Get<ViewSpecShadingParameters>();

	const rg::RGTextureViewHandle indirectInScattering = indirect::Render(graphBuilder, renderScene, viewSpec, fogParams);

	VolumetricFogInScatteringParams inScatteringParams;
	inScatteringParams.jitter                              = fogParams.fogJitter;
	inScatteringParams.localLightsPhaseFunctionAnisotrophy = parameters::localLightsPhaseFunctionAnisotrophy;
	inScatteringParams.dirLightsPhaseFunctionAnisotrophy   = parameters::dirLightsPhaseFunctionAnisotrophy;
	inScatteringParams.hasValidHistory                     = fogParams.inScatteringHistoryTextureView.IsValid();
	inScatteringParams.accumulationCurrentFrameWeight      = 0.05f;
	inScatteringParams.enableDirectionalLightsInScattering = parameters::enableDirectionalLightsInScattering;
	inScatteringParams.fogNearPlane                        = fogParams.nearPlane;
	inScatteringParams.fogFarPlane                         = fogParams.farPlane;
	inScatteringParams.enableIndirectInScattering          = indirectInScattering.IsValid();

	const lib::MTHandle<ComputeInScatteringDS> computeInScatteringDS = graphBuilder.CreateDescriptorSet<ComputeInScatteringDS>(RENDERER_RESOURCE_NAME("ComputeInScatteringDS"));
	computeInScatteringDS->u_participatingMediaTexture   = fogParams.participatingMediaTextureView;
	computeInScatteringDS->u_inScatteringTexture         = fogParams.inScatteringTextureView;
	computeInScatteringDS->u_inScatteringHistoryTexture  = fogParams.inScatteringHistoryTextureView;
	computeInScatteringDS->u_inScatteringParams          = inScatteringParams;
	computeInScatteringDS->u_indirectInScatteringTexture = indirectInScattering;

	const lib::SharedPtr<DDGISceneSubsystem> ddgiSubsystem = renderScene.GetSceneSubsystem<DDGISceneSubsystem>();
	lib::MTHandle<DDGIDS> ddgiDS = ddgiSubsystem ? ddgiSubsystem->GetDDGIDS() : nullptr;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(computeInScatteringDS,
												 renderView.GetRenderViewDS(),
												 shadingParams.shadingInputDS,
												 shadingParams.shadowMapsDS,
												 std::move(ddgiDS)));
}

} // in_scattering

namespace integrate_in_scattering
{

BEGIN_SHADER_STRUCT(IntegrateInScatteringParams)
	SHADER_STRUCT_FIELD(Real32, fogNearPlane)
	SHADER_STRUCT_FIELD(Real32, fogFarPlane)
END_SHADER_STRUCT();

DS_BEGIN(IntegrateInScatteringDS, rg::RGDescriptorSetState<IntegrateInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),								u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_inScatteringSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),								u_integratedInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<IntegrateInScatteringParams>),		u_integrateInScatteringParams)
DS_END();


static rdr::PipelineStateID CompileIntegrateInScatteringPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/IntegrateInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "IntegrateInScatteringCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("IntegrateInScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	IntegrateInScatteringParams params;
	params.fogNearPlane	= fogParams.nearPlane;
	params.fogFarPlane	= fogParams.farPlane;

	const lib::MTHandle<IntegrateInScatteringDS> computeInScatteringDS = graphBuilder.CreateDescriptorSet<IntegrateInScatteringDS>(RENDERER_RESOURCE_NAME("IntegrateInScatteringDS"));
	computeInScatteringDS->u_inScatteringTexture			= fogParams.inScatteringTextureView;
	computeInScatteringDS->u_integratedInScatteringTexture	= fogParams.integratedInScatteringTextureView;
	computeInScatteringDS->u_integrateInScatteringParams	= params;

	const math::Vector3u dispatchElements = math::Vector3u(fogParams.volumetricFogResolution.x(), fogParams.volumetricFogResolution.y(), 1u);
	const math::Vector3u dispatchSize = math::Utils::DivideCeil(dispatchElements, math::Vector3u(8u, 8u, 1u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileIntegrateInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Integrate In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(computeInScatteringDS, renderView.GetRenderViewDS()));
}

} // integrate_in_scattering


ParticipatingMediaViewRenderSystem::ParticipatingMediaViewRenderSystem()
{
}

void ParticipatingMediaViewRenderSystem::PreRenderFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::PreRenderFrame(graphBuilder, renderScene, viewSpec);

	SPT_CHECK(viewSpec.SupportsStage(ERenderStage::ForwardOpaque));
	viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque).GetPostRenderStage().AddRawMember(this, &ParticipatingMediaViewRenderSystem::RenderParticipatingMedia);
}

const VolumetricFogParams& ParticipatingMediaViewRenderSystem::GetVolumetricFogParams() const
{
	return m_volumetricFogParams;
}

void ParticipatingMediaViewRenderSystem::RenderParticipatingMedia(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const rsc::RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingResolution();
	
	const Uint32 volumetricTileSize = 8u;

	const math::Vector2u volumetricTileRes(volumetricTileSize, volumetricTileSize);
	const math::Vector2u volumetricTilesNum = math::Utils::DivideCeil(renderingRes, volumetricTileRes);

	const Uint32 volumetricFogZRes = 128u;

	const math::Vector3u volumetricFogRes(volumetricTilesNum.x(), volumetricTilesNum.y(), volumetricFogZRes);

	rg::TextureDef participatingMediaTextureDef;
	participatingMediaTextureDef.resolution	= volumetricFogRes;
	participatingMediaTextureDef.format		= rhi::EFragmentFormat::RGBA16_UN_Float; // [Color (RGB), Extinction (A)]
	const rg::RGTextureViewHandle participatingMediaTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Participating Media Texture"), participatingMediaTextureDef);

	rg::TextureDef integratedInScatteringTextureDef;
	integratedInScatteringTextureDef.resolution	= volumetricFogRes;
	integratedInScatteringTextureDef.format		= rhi::EFragmentFormat::RGBA16_S_Float; // [Integrated In-Scattering (RGB), Transmittance (A)]
	const rg::RGTextureViewHandle integratedInScatteringTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Integrated In-Scattering Texture"), integratedInScatteringTextureDef);

	PrepareViewTextures(volumetricFogRes);
	
	SPT_CHECK(!!m_currentInScatteringTexture);
	const rg::RGTextureViewHandle inScatteringTextureView = graphBuilder.AcquireExternalTextureView(m_currentInScatteringTexture);
	
	rg::RGTextureViewHandle inScatteringHistoryTextureView;
	if (m_prevFrameInScatteringTexture)
	{
		inScatteringHistoryTextureView = graphBuilder.AcquireExternalTextureView(m_prevFrameInScatteringTexture);
	}

	const Uint64 frameIdx = engn::GetRenderingFrame().GetFrameIdx();
	const Uint32 jitterSequenceIdx = static_cast<Uint32>(frameIdx) & 7u;

	const math::Vector3f jitterSequence = math::Vector3f(math::Sequences::Halton<Real32>(jitterSequenceIdx, 2),
														 math::Sequences::Halton<Real32>(jitterSequenceIdx, 3),
														 math::Sequences::Halton<Real32>(jitterSequenceIdx, 4));

	const math::Vector3f jitter = (jitterSequence - math::Vector3f::Constant(1.0f)).cwiseProduct(volumetricFogRes.cast<Real32>().cwiseInverse());

	m_volumetricFogParams = VolumetricFogParams{};
	m_volumetricFogParams.participatingMediaTextureView			= participatingMediaTextureView;
	m_volumetricFogParams.inScatteringTextureView				= inScatteringTextureView;
	m_volumetricFogParams.integratedInScatteringTextureView		= integratedInScatteringTextureView;
	m_volumetricFogParams.inScatteringHistoryTextureView		= inScatteringHistoryTextureView;
	m_volumetricFogParams.volumetricFogResolution				= volumetricFogRes;
	m_volumetricFogParams.fogJitter								= jitter;
	m_volumetricFogParams.nearPlane								= renderView.GetNearPlane();
	m_volumetricFogParams.farPlane								= parameters::fogFarPlane;

	participating_media::Render(graphBuilder, renderScene, viewSpec, m_volumetricFogParams);

	in_scattering::Render(graphBuilder, renderScene, viewSpec, m_volumetricFogParams);

	integrate_in_scattering::Render(graphBuilder, renderScene, viewSpec, m_volumetricFogParams);

	std::swap(m_currentInScatteringTexture, m_prevFrameInScatteringTexture);
}

void ParticipatingMediaViewRenderSystem::PrepareViewTextures(const math::Vector3u& volumetricFogResolution)
{
	if (!m_currentInScatteringTexture || m_currentInScatteringTexture->GetResolution() != volumetricFogResolution)
	{
		rhi::TextureDefinition inScatteringTextureDef;
		inScatteringTextureDef.resolution	= volumetricFogResolution;
		inScatteringTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
		inScatteringTextureDef.format		= rhi::EFragmentFormat::RGBA16_S_Float; // [In-Scattering (RGB), Extinction (A)]
		m_currentInScatteringTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("In-Scattering Texture"), inScatteringTextureDef, rhi::EMemoryUsage::GPUOnly);
	}
}

} // spt::rsc
