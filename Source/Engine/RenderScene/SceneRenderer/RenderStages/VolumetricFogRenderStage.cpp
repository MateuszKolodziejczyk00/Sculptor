#include "VolumetricFogRenderStage.h"
#include "RenderGraphBuilder.h"
#include "Common/ShaderCompilationInput.h"
#include "ResourcesManager.h"
#include "Lights/ViewShadingInput.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "EngineFrame.h"
#include "Sequences.h"
#include "RenderScene.h"

namespace spt::rsc
{

namespace parameters
{

RendererFloatParameter fogMinDensity("Fog Min Density", { "Volumetric Fog" }, 0.02f, 0.f, 1.f);
RendererFloatParameter fogMaxDensity("Fog Max Density", { "Volumetric Fog" }, 0.6f, 0.f, 1.f);
RendererFloatParameter scatteringFactor("Scattering Factor", { "Volumetric Fog" }, 0.1f, 0.f, 1.f);
RendererFloatParameter localLightsPhaseFunctionAnisotrophy("Local Lights Phase Function Aniso", { "Volumetric Fog" }, 0.4f, 0.f, 1.f);
RendererFloatParameter dirLightsPhaseFunctionAnisotrophy("Directional Lights Phase Function Aniso", { "Volumetric Fog" }, 0.2f, 0.f, 1.f);

RendererFloatParameter fogFarPlane("Fog Far Plane", { "Volumetric Fog" }, 20.f, 1.f, 50.f);

RendererBoolParameter enableDirectionalLightsInScattering("Enable Directional Lights Scattering", { "Volumetric Fog" }, true);
RendererBoolParameter enableDirectionalLightsVolumetricRTShadows("Enable Directional Lights Volumetric RT Shadows", { "Volumetric Fog" }, true);

} // parameters


struct VolumetricFogParams 
{
	VolumetricFogParams()
		: volumetricFogResolution{}
		, fogJitter{}
		, nearPlane(0.f)
		, farPlane(0.f)
	{ }

	rg::RGTextureViewHandle participatingMediaTextureView;
	rg::RGTextureViewHandle inScatteringTextureView;
	rg::RGTextureViewHandle integratedInScatteringTextureView;

	rg::RGTextureViewHandle inScatteringHistoryTextureView;

	rg::RGTextureViewHandle depthTextureView;

	math::Vector3u volumetricFogResolution;

	math::Vector3f fogJitter;

	Real32 nearPlane;
	Real32 farPlane;
};


struct SceneViewVolumetricDataComponent
{
	lib::SharedPtr<rdr::TextureView> currentInScatteringTexture;
	lib::SharedPtr<rdr::TextureView> prevFrameInScatteringTexture;
};


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

	const lib::SharedRef<RenderParticipatingMediaDS> participatingMediaDS = rdr::ResourcesManager::CreateDescriptorSetState<RenderParticipatingMediaDS>(RENDERER_RESOURCE_NAME("RenderParticipatingMediaDS"));
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

BEGIN_SHADER_STRUCT(VolumetricFogInScatteringParams)
	SHADER_STRUCT_FIELD(math::Vector3f,	jitter)
	SHADER_STRUCT_FIELD(Real32,			localLightsPhaseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Real32,			dirLightsPhaseFunctionAnisotrophy)
	SHADER_STRUCT_FIELD(Real32,			fogNearPlane)
	SHADER_STRUCT_FIELD(Real32,			fogFarPlane)
	SHADER_STRUCT_FIELD(Bool,			hasValidHistory)
	SHADER_STRUCT_FIELD(Real32,			accumulationCurrentFrameWeight)
	SHADER_STRUCT_FIELD(Real32,			enableDirectionalLightsInScattering)
END_SHADER_STRUCT();


DS_BEGIN(ComputeInScatteringDS, rg::RGDescriptorSetState<ComputeInScatteringDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),								u_participatingMediaTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_participatingMediaSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),								u_inScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector4f>),						u_inScatteringHistoryTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_inScatteringHistorySampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<VolumetricFogInScatteringParams>),	u_inScatteringParams)
DS_END();


BEGIN_SHADER_STRUCT(VolumetricRayTracedShadowsParams)
	SHADER_STRUCT_FIELD(Real32,	shadowRayMinT)
	SHADER_STRUCT_FIELD(Real32, shadowRayMaxT)
	SHADER_STRUCT_FIELD(Bool,	enableDirectionalLightsVolumetricRTShadows)
END_SHADER_STRUCT();


DS_BEGIN(InScatteringAccelerationStructureDS, rg::RGDescriptorSetState<InScatteringAccelerationStructureDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),											u_sceneAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<VolumetricRayTracedShadowsParams>),		u_volumetricRayTracedShadowsParams)
DS_END();


static rdr::PipelineStateID CompileComputeInScatteringPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("ENABLE_RAY_TRACING", "1"));
		compilationSettings.DisableGeneratingDebugSource();
	}
	else
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("ENABLE_RAY_TRACING", "0"));
	}
	
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ComputeInScattering.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeInScatteringCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("InScatteringPipeline"), shader);
}

static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ViewSpecShadingParameters& shadingParams = viewSpec.GetData().Get<ViewSpecShadingParameters>();

	VolumetricFogInScatteringParams inScatteringParams;
	inScatteringParams.jitter								= fogParams.fogJitter;
	inScatteringParams.localLightsPhaseFunctionAnisotrophy	= parameters::localLightsPhaseFunctionAnisotrophy;
	inScatteringParams.dirLightsPhaseFunctionAnisotrophy	= parameters::dirLightsPhaseFunctionAnisotrophy;
	inScatteringParams.hasValidHistory						= fogParams.inScatteringHistoryTextureView.IsValid();
	inScatteringParams.accumulationCurrentFrameWeight		= 0.05f;
	inScatteringParams.enableDirectionalLightsInScattering	= parameters::enableDirectionalLightsInScattering;
	inScatteringParams.fogNearPlane							= fogParams.nearPlane;
	inScatteringParams.fogFarPlane							= fogParams.farPlane;

	const lib::SharedRef<ComputeInScatteringDS> computeInScatteringDS = rdr::ResourcesManager::CreateDescriptorSetState<ComputeInScatteringDS>(RENDERER_RESOURCE_NAME("ComputeInScatteringDS"));
	computeInScatteringDS->u_participatingMediaTexture	= fogParams.participatingMediaTextureView;
	computeInScatteringDS->u_inScatteringTexture		= fogParams.inScatteringTextureView;
	computeInScatteringDS->u_inScatteringHistoryTexture	= fogParams.inScatteringHistoryTextureView;
	computeInScatteringDS->u_depthTexture				= fogParams.depthTextureView;
	computeInScatteringDS->u_inScatteringParams			= inScatteringParams;

	lib::SharedPtr<InScatteringAccelerationStructureDS> accelerationStructureDS;
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		RayTracingRenderSceneSubsystem& rayTracingSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

		VolumetricRayTracedShadowsParams volumetricRayTracedShadowsParams;
		volumetricRayTracedShadowsParams.shadowRayMinT								= 0.04f;
		volumetricRayTracedShadowsParams.shadowRayMaxT								= 30.f;
		volumetricRayTracedShadowsParams.enableDirectionalLightsVolumetricRTShadows	= parameters::enableDirectionalLightsVolumetricRTShadows;

		accelerationStructureDS = rdr::ResourcesManager::CreateDescriptorSetState<InScatteringAccelerationStructureDS>(RENDERER_RESOURCE_NAME("InScatteringAccelerationStructureDS"));
		accelerationStructureDS->u_sceneAccelerationStructure		= lib::Ref(rayTracingSubsystem.GetSceneTLAS());
		accelerationStructureDS->u_volumetricRayTracedShadowsParams	= volumetricRayTracedShadowsParams;
	}

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(fogParams.volumetricFogResolution, math::Vector3u(4u, 4u, 4u));

	static const rdr::PipelineStateID computeInScatteringPipeline = CompileComputeInScatteringPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compute In-Scattering"),
						  computeInScatteringPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(computeInScatteringDS,
												 renderView.GetRenderViewDS(),
												 shadingParams.shadingInputDS,
												 shadingParams.shadowMapsDS,
												 accelerationStructureDS));
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
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
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

	const lib::SharedRef<IntegrateInScatteringDS> computeInScatteringDS = rdr::ResourcesManager::CreateDescriptorSetState<IntegrateInScatteringDS>(RENDERER_RESOURCE_NAME("IntegrateInScatteringDS"));
	computeInScatteringDS->u_inScatteringTexture			= fogParams.inScatteringTextureView;
	computeInScatteringDS->u_integratedInScatteringTexture	= fogParams.integratedInScatteringTextureView;
	computeInScatteringDS->u_depthTexture					= fogParams.depthTextureView;
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

namespace apply_volumetric_fog
{

BEGIN_SHADER_STRUCT(ApplyVolumetricFogParams)
	SHADER_STRUCT_FIELD(Real32, fogNearPlane)
	SHADER_STRUCT_FIELD(Real32, fogFarPlane)
	SHADER_STRUCT_FIELD(Real32, blendPixelsOffset)
END_SHADER_STRUCT();


DS_BEGIN(ApplyVolumetricFogDS, rg::RGDescriptorSetState<ApplyVolumetricFogDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),								u_integratedInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_integratedInScatteringSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<ApplyVolumetricFogParams>),			u_applyVolumetricFogParams)
DS_END();


static rdr::PipelineStateID CompileApplyVolumetricFogPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/VolumetricFog/ApplyVolumetricFog.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ApplyVolumetricFogCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ApplyVolumetricFogPipeline"), shader);
}


static void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const ViewRenderingSpec& viewSpec, const VolumetricFogParams& fogParams)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
	const ShadingData& shadingData				= viewSpec.GetData().Get<ShadingData>();

	ApplyVolumetricFogParams params;
	params.fogNearPlane			= fogParams.nearPlane;
	params.fogFarPlane			= fogParams.farPlane;
	params.blendPixelsOffset	= 8.f;

	const lib::SharedRef<ApplyVolumetricFogDS> applyVolumetricFogDS = rdr::ResourcesManager::CreateDescriptorSetState<ApplyVolumetricFogDS>(RENDERER_RESOURCE_NAME("ApplyVolumetricFogDS"));
	applyVolumetricFogDS->u_integratedInScatteringTexture	= fogParams.integratedInScatteringTextureView;
	applyVolumetricFogDS->u_luminanceTexture				= shadingData.luminanceTexture;
	applyVolumetricFogDS->u_depthTexture					= depthPrepassData.depth;
	applyVolumetricFogDS->u_applyVolumetricFogParams		= params;

	const math::Vector3u dispatchSize = math::Utils::DivideCeil(renderView.GetRenderingResolution3D(), math::Vector3u(8u, 8u, 1u));

	static const rdr::PipelineStateID applyVolumetricFogPipeline = CompileApplyVolumetricFogPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Volumetric Fog"),
						  applyVolumetricFogPipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(applyVolumetricFogDS, renderView.GetRenderViewDS()));
}

} // apply_volumetric_fog

rg::RGTextureViewHandle CreateHiZProperMipViewForVolumetricFog(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, Uint32 volumetricTileSize)
{
	SPT_CHECK(math::Utils::IsPowerOf2(volumetricTileSize));

	const DepthPrepassData& depthPrepassData = viewSpec.GetData().Get<DepthPrepassData>();
	SPT_CHECK(depthPrepassData.hiZ.IsValid());

	const rg::RGTextureHandle hiZTexture = depthPrepassData.hiZ->GetTexture();

	// We need to use greater mip to avoid artifacts with blending.
	// The reason is that we use linear filtering for integrated in scattering texture, so we may access neighbor froxels that are occluded
	// This still may not be enough and we may have lower scattering and transmittance values near occluded froxels but it's acceptable
	const Uint32 depthPyramidMipIdx = std::min(math::Utils::LowestSetBitIdx(volumetricTileSize) + 1, hiZTexture->GetMipLevelsNum() - 1);

	rhi::TextureViewDefinition hiZProperMipViewDef;
	hiZProperMipViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, depthPyramidMipIdx, 1, 0, 1);
	
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("HiZ Volumetric Fog View"), hiZTexture, hiZProperMipViewDef);
}

SceneViewVolumetricDataComponent& GetVolumetricDataForView(RenderSceneEntityHandle viewEntity, const math::Vector3u& volumetricFogResolution)
{
	SceneViewVolumetricDataComponent* volumetricDataComp = viewEntity.try_get<SceneViewVolumetricDataComponent>();
	if (!volumetricDataComp)
	{
		volumetricDataComp = &viewEntity.emplace<SceneViewVolumetricDataComponent>();
	}

	if (!volumetricDataComp->currentInScatteringTexture || volumetricDataComp->currentInScatteringTexture->GetResolution() != volumetricFogResolution)
	{
		rhi::TextureDefinition inScatteringTextureDef;
		inScatteringTextureDef.resolution	= volumetricFogResolution;
		inScatteringTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
		inScatteringTextureDef.format		= rhi::EFragmentFormat::RGBA16_S_Float; // [In-Scattering (RGB), Extinction (A)]
		const lib::SharedRef<rdr::Texture> inScatteringTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("In-Scattering Texture"), inScatteringTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureViewDefinition textureViewDef;
		textureViewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		volumetricDataComp->currentInScatteringTexture = inScatteringTexture->CreateView(RENDERER_RESOURCE_NAME("In-Scattering Texture View"), textureViewDef);
	}

	return *volumetricDataComp;
}

VolumetricFogRenderStage::VolumetricFogRenderStage()
{ }

void VolumetricFogRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
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

	const RenderSceneEntityHandle renderViewEntity = renderView.GetViewEntity();
	SceneViewVolumetricDataComponent& volumetricDataComp = GetVolumetricDataForView(renderViewEntity, volumetricFogRes);
	
	SPT_CHECK(!!volumetricDataComp.currentInScatteringTexture);
	const rg::RGTextureViewHandle inScatteringTextureView = graphBuilder.AcquireExternalTextureView(volumetricDataComp.currentInScatteringTexture);
	
	rg::RGTextureViewHandle inScatteringHistoryTextureView;
	if (volumetricDataComp.prevFrameInScatteringTexture)
	{
		inScatteringHistoryTextureView = graphBuilder.AcquireExternalTextureView(volumetricDataComp.prevFrameInScatteringTexture);
	}

	const Uint64 frameIdx = engn::GetRenderingFrame().GetFrameIdx();
	const Uint32 jitterSequenceIdx = static_cast<Uint32>(frameIdx) & 7u;

	const math::Vector3f jitterSequence = math::Vector3f(math::Sequences::Halton<Real32>(jitterSequenceIdx, 2),
														 math::Sequences::Halton<Real32>(jitterSequenceIdx, 3),
														 math::Sequences::Halton<Real32>(jitterSequenceIdx, 4));

	const math::Vector3f jitter = (jitterSequence - math::Vector3f::Constant(1.0f)).cwiseProduct(volumetricFogRes.cast<Real32>().cwiseInverse());

	VolumetricFogParams fogParams;
	fogParams.participatingMediaTextureView			= participatingMediaTextureView;
	fogParams.inScatteringTextureView				= inScatteringTextureView;
	fogParams.integratedInScatteringTextureView		= integratedInScatteringTextureView;
	fogParams.inScatteringHistoryTextureView		= inScatteringHistoryTextureView;
	fogParams.depthTextureView						= CreateHiZProperMipViewForVolumetricFog(graphBuilder, viewSpec, volumetricTileSize);
	fogParams.volumetricFogResolution				= volumetricFogRes;
	fogParams.fogJitter								= jitter;
	fogParams.nearPlane								= renderView.GetNearPlane();
	fogParams.farPlane								= parameters::fogFarPlane;

	participating_media::Render(graphBuilder, renderScene, viewSpec, fogParams);

	in_scattering::Render(graphBuilder, renderScene, viewSpec, fogParams);

	integrate_in_scattering::Render(graphBuilder, renderScene, viewSpec, fogParams);

	apply_volumetric_fog::Render(graphBuilder, renderScene, viewSpec, fogParams);

	std::swap(volumetricDataComp.currentInScatteringTexture, volumetricDataComp.prevFrameInScatteringTexture);

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
