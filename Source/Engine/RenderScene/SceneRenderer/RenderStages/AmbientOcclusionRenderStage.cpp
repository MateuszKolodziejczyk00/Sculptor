#include "AmbientOcclusionRenderStage.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "Pipelines/PipelineState.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "Utility/Random.h"
#include "Denoisers/AmbientOcclusionDenoiser.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"
#include "MaterialsUnifiedData.h"
#include "SceneRenderer/Utils/RTVisibilityUtils.h"
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"


namespace spt::rsc
{

namespace rtao
{

struct RTAOViewDataComponent
{
	lib::SharedPtr<rdr::TextureView> currentAOTexture;
	lib::SharedPtr<rdr::TextureView> historyAOTexture;
};


struct RTAOViewRenderingData
{
	rg::RGTextureViewHandle currentAOTexture;
	rg::RGTextureViewHandle historyAOTexture;
};


struct AORenderingContext
{
	const RenderScene& renderScene;
	const RenderView& renderView;
	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle depthTextureHalfRes;
	rg::RGTextureViewHandle historyDepthTexture;
	rg::RGTextureViewHandle geometryNormalsTextureHalfRes;
	rg::RGTextureViewHandle motionTextureHalfRes;
};

namespace trace_rays
{

BEGIN_SHADER_STRUCT(RTAOTraceRaysParams)
	SHADER_STRUCT_FIELD(math::Vector2f,	randomSeed)
	SHADER_STRUCT_FIELD(Uint32,			raysNumber)
	SHADER_STRUCT_FIELD(Real32,			raysLength)
	SHADER_STRUCT_FIELD(Real32,			raysMinHitDistance)
END_SHADER_STRUCT();


DS_BEGIN(RTAOTraceRaysDS, rg::RGDescriptorSetState<RTAOTraceRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_geometryNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_ambientOcclusionTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<RTAOTraceRaysParams>),				u_rtaoParams)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),										u_worldAccelerationStructure)
DS_END();


static rdr::PipelineStateID CreateShadowsRayTracingPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();

	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateAmbientOcclusionRaysRTG"), compilationSettings);
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "RTVIsibilityRTM"), compilationSettings));

	const lib::DynamicArray<mat::MaterialShadersHash>& sbtRecords = rayTracingSubsystem.GetMaterialShaderSBTRecords();

	const lib::HashedString materialTechnique = "RTVisibility";

	for (SizeType recordIdx = 0; recordIdx < sbtRecords.size(); ++recordIdx)
	{
		const mat::MaterialRayTracingShaders shaders = mat::MaterialsSubsystem::Get().GetMaterialShaders<mat::MaterialRayTracingShaders>(materialTechnique, sbtRecords[recordIdx]);

		rdr::RayTracingHitGroup hitGroup;
		hitGroup.closestHitShader	= shaders.closestHitShader;
		hitGroup.anyHitShader		= shaders.anyHitShader;

		rtShaders.hitGroups.emplace_back(hitGroup);
	}

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1u;

	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("RTAO Trace Rays Pipeline"), rtShaders, pipelineDefinition);
}


static void TraceAmbientOcclusionRays(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context, const RTAOViewRenderingData& viewRenderingData)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = viewRenderingData.currentAOTexture->GetResolution();

	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = context.renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	static rdr::PipelineStateID traceRaysPipeline;
	if (!traceRaysPipeline.IsValid() || rayTracingSceneSubsystem.AreSBTRecordsDirty())
	{
		traceRaysPipeline = CreateShadowsRayTracingPipeline(rayTracingSceneSubsystem);
	}

	RTAOTraceRaysParams params;
	params.randomSeed			= math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	params.raysNumber			= 1u;
	params.raysLength			= 1.0f;
	params.raysMinHitDistance	= 0.02f;

	lib::SharedPtr<RTAOTraceRaysDS> traceRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<RTAOTraceRaysDS>(RENDERER_RESOURCE_NAME("RTAOTraceRaysDS"));
	traceRaysDS->u_depthTexture					= context.depthTextureHalfRes;
	traceRaysDS->u_geometryNormalsTexture		= context.geometryNormalsTextureHalfRes;
	traceRaysDS->u_ambientOcclusionTexture		= viewRenderingData.currentAOTexture;
	traceRaysDS->u_rtaoParams					= params;
	traceRaysDS->u_worldAccelerationStructure	= lib::Ref(rayTracingSceneSubsystem.GetSceneTLAS());

	const lib::SharedPtr<RTVisibilityDS> visibilityDS = rdr::ResourcesManager::CreateDescriptorSetState<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
	visibilityDS->u_rtInstances				= rayTracingSceneSubsystem.GetRTInstancesDataBuffer()->CreateFullView();
	visibilityDS->u_geometryDS				= GeometryManager::Get().GetGeometryDSState();
	visibilityDS->u_staticMeshUnifiedDataDS	= StaticMeshUnifiedData::Get().GetUnifiedDataDS();
	visibilityDS->u_MaterialsDS				= mat::MaterialsUnifiedData::Get().GetMaterialsDS();

	graphBuilder.TraceRays(RG_DEBUG_NAME("RTAO Trace Rays"),
						   traceRaysPipeline,
						   resolution,
						   rg::BindDescriptorSets(std::move(traceRaysDS),
												  std::move(visibilityDS),
												  context.renderView.GetRenderViewDS()));
}

} // trace_rays

static lib::SharedRef<rdr::TextureView> CreateAOTexture(const math::Vector3u& renderingResolution)
{
	const rhi::EFragmentFormat format = rhi::EFragmentFormat::R16_UN_Float;
	rhi::TextureDefinition textureDefinition(renderingResolution, lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture), format);
#if !SPT_RELEASE
	lib::AddFlag(textureDefinition.usage, rhi::ETextureUsage::TransferSource);
#endif // SPT_RELEASE
	const lib::SharedRef<rdr::Texture> texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("AO Temporal Texture"), textureDefinition, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
	return texture->CreateView(RENDERER_RESOURCE_NAME("AO Temporal Texture View"), viewDefinition);
}


static rg::RGTextureViewHandle RenderAO(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = context.renderView;
	const math::Vector3u aoRenderingResolution = context.depthTextureHalfRes->GetResolution();

	const RenderSceneEntityHandle& viewEntity = renderView.GetViewEntity();
	RTAOViewDataComponent* viewTemporalData = viewEntity.try_get<RTAOViewDataComponent>();
	if (!viewTemporalData)
	{
		viewTemporalData = &viewEntity.emplace<RTAOViewDataComponent>();
	}

	lib::SharedPtr<rdr::TextureView>& historyTexture = viewTemporalData->historyAOTexture;
	lib::SharedPtr<rdr::TextureView>& currentTexture = viewTemporalData->currentAOTexture;

	const Bool hasValidHistory = historyTexture && historyTexture->GetResolution() == aoRenderingResolution;

	if (!currentTexture || currentTexture->GetResolution() != aoRenderingResolution)
	{
		currentTexture = CreateAOTexture(aoRenderingResolution);
	}

	if (!hasValidHistory)
	{
		historyTexture = CreateAOTexture(aoRenderingResolution);
	}

	RTAOViewRenderingData viewAORenderingData;
	viewAORenderingData.historyAOTexture = historyTexture ? graphBuilder.AcquireExternalTextureView(historyTexture) : nullptr;
	viewAORenderingData.currentAOTexture = graphBuilder.AcquireExternalTextureView(currentTexture);

	trace_rays::TraceAmbientOcclusionRays(graphBuilder, context, viewAORenderingData);

	ao_denoiser::AODenoiserParams denoiserParams(renderView);
	denoiserParams.name						= RG_DEBUG_NAME("RTAO");
	denoiserParams.historyDepthTexture		= context.historyDepthTexture;
	denoiserParams.currentDepthTexture		= context.depthTextureHalfRes;
	denoiserParams.motionTexture			= context.motionTextureHalfRes;
	denoiserParams.geometryNormalsTexture	= context.geometryNormalsTextureHalfRes;
	denoiserParams.currentTexture			= viewAORenderingData.currentAOTexture;
	denoiserParams.historyTexture			= viewAORenderingData.historyAOTexture;
	denoiserParams.enableTemporalFilter		= hasValidHistory;

	ao_denoiser::Denoise(graphBuilder, denoiserParams);

	const math::Vector2u frameRenderingRes = renderView.GetRenderingResolution();

	upsampler::DepthBasedUpsampleParams upsampleParams;
	upsampleParams.debugName = RG_DEBUG_NAME("RTAO Upsample");
	upsampleParams.depth = context.depthTexture;
	upsampleParams.depthHalfRes = context.depthTextureHalfRes;
	upsampleParams.renderViewDS = renderView.GetRenderViewDS();
	return upsampler::DepthBasedUpsample(graphBuilder, viewAORenderingData.currentAOTexture, upsampleParams);
}

} // rtao

AmbientOcclusionRenderStage::AmbientOcclusionRenderStage()
{ }

void AmbientOcclusionRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const DepthPrepassData& depthPrepassData = viewSpec.GetData().Get<DepthPrepassData>();
	SPT_CHECK(depthPrepassData.depthHalfRes.IsValid());

	const MotionData& motionData = viewSpec.GetData().Get<MotionData>();
	SPT_CHECK(motionData.motionHalfRes.IsValid());

	ShadingInputData& shadingInputData = viewSpec.GetData().Get<ShadingInputData>();
	SPT_CHECK(shadingInputData.geometryNormalsHalfRes.IsValid());

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		const RenderView& renderView = viewSpec.GetRenderView();

		rtao::AORenderingContext aoContext{ renderScene, renderView };
		aoContext.geometryNormalsTextureHalfRes = shadingInputData.geometryNormalsHalfRes;
		aoContext.depthTexture					= depthPrepassData.depth;
		aoContext.depthTextureHalfRes			= depthPrepassData.depthHalfRes;
		aoContext.historyDepthTexture			= depthPrepassData.prevFrameDepth;
		aoContext.motionTextureHalfRes			= motionData.motionHalfRes;

		const rg::RGTextureViewHandle aoTextureView = rtao::RenderAO(graphBuilder, aoContext);
		shadingInputData.ambientOcclusion = aoTextureView;
	}

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
