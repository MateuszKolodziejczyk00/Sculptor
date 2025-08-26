#include "AmbientOcclusionRenderStage.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "Pipelines/PipelineState.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "Utility/Random.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"
#include "MaterialsUnifiedData.h"
#include "SceneRenderer/Utils/RTVisibilityUtils.h"
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::AmbientOcclusion, AmbientOcclusionRenderStage);


namespace rtao
{

struct AORenderingContext
{
	const RenderScene&             renderScene;
	const RenderView&              renderView;
	visibility_denoiser::Denoiser& denoiser;
	rg::RGTextureViewHandle        depthTexture;
	rg::RGTextureViewHandle        depthTextureHalfRes;
	rg::RGTextureViewHandle        historyDepthTextureHalfRes;
	rg::RGTextureViewHandle        normalsTextureHalfRes;
	rg::RGTextureViewHandle        motionTextureHalfRes;
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
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_ambientOcclusionTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RTAOTraceRaysParams>),                     u_rtaoParams)
DS_END();


static rdr::PipelineStateID CreateAORayTracingPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateAmbientOcclusionRaysRTG"));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "RTVisibilityRTM")));

	const lib::HashedString materialTechnique = "RTVisibility";
	rayTracingSubsystem.FillRayTracingGeometryHitGroups(materialTechnique, INOUT rtShaders.hitGroups);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1u;

	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("RTAO Trace Rays Pipeline"), rtShaders, pipelineDefinition);
}


static rg::RGTextureViewHandle TraceAmbientOcclusionRays(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context, const math::Vector2u traceRaysResolution)
{
	SPT_PROFILER_FUNCTION();

	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = context.renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	static rdr::PipelineStateID traceRaysPipeline;
	if (!traceRaysPipeline.IsValid() || rayTracingSceneSubsystem.AreSBTRecordsDirty())
	{
		traceRaysPipeline = CreateAORayTracingPipeline(rayTracingSceneSubsystem);
	}
	
	const rg::RGTextureViewHandle traceRaysResultTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("AO Trace Rays Result"), rg::TextureDef(traceRaysResolution, rhi::EFragmentFormat::R8_UN_Float));

	RTAOTraceRaysParams params;
	params.randomSeed			= math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	params.raysNumber			= 1u;
	params.raysLength			= 0.25f;
	params.raysMinHitDistance	= 0.02f;

	lib::MTHandle<RTAOTraceRaysDS> traceRaysDS = graphBuilder.CreateDescriptorSet<RTAOTraceRaysDS>(RENDERER_RESOURCE_NAME("RTAOTraceRaysDS"));
	traceRaysDS->u_depthTexture               = context.depthTextureHalfRes;
	traceRaysDS->u_normalsTexture             = context.normalsTextureHalfRes;
	traceRaysDS->u_ambientOcclusionTexture    = traceRaysResultTexture;
	traceRaysDS->u_rtaoParams                 = params;

	lib::MTHandle<RTVisibilityDS> visibilityDS = graphBuilder.CreateDescriptorSet<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
	visibilityDS->u_geometryDS              = GeometryManager::Get().GetGeometryDSState();
	visibilityDS->u_staticMeshUnifiedDataDS = StaticMeshUnifiedData::Get().GetUnifiedDataDS();
	visibilityDS->u_materialsDS             = mat::MaterialsUnifiedData::Get().GetMaterialsDS();
	visibilityDS->u_sceneRayTracingDS       = rayTracingSceneSubsystem.GetSceneRayTracingDS();

	graphBuilder.TraceRays(RG_DEBUG_NAME("RTAO Trace Rays"),
						   traceRaysPipeline,
						   traceRaysResolution,
						   rg::BindDescriptorSets(std::move(traceRaysDS),
												  std::move(visibilityDS),
												  context.renderView.GetRenderViewDS()));

	return traceRaysResultTexture;
}

} // trace_rays

static rg::RGTextureViewHandle RenderAO(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RTAO");

	const RenderView& renderView = context.renderView;
	const math::Vector2u aoRenderingResolution = context.depthTextureHalfRes->GetResolution2D();

	const rg::RGTextureViewHandle aoHalfResTexture = trace_rays::TraceAmbientOcclusionRays(graphBuilder, context, aoRenderingResolution);

	visibility_denoiser::Denoiser::Params denoiserParams(renderView);
	denoiserParams.historyDepthTexture       = context.historyDepthTextureHalfRes;
	denoiserParams.currentDepthTexture       = context.depthTextureHalfRes;
	denoiserParams.motionTexture             = context.motionTextureHalfRes;
	denoiserParams.normalsTexture            = context.normalsTextureHalfRes;
	denoiserParams.currentFrameDefaultWeight = 0.045f;
	denoiserParams.accumulatedFramesMaxCount = 32.f;
	context.denoiser.Denoise(graphBuilder, aoHalfResTexture, denoiserParams);

	upsampler::DepthBasedUpsampleParams upsampleParams;
	upsampleParams.debugName      = RG_DEBUG_NAME("RTAO Upsample");
	upsampleParams.depth          = context.depthTexture;
	upsampleParams.depthHalfRes   = context.depthTextureHalfRes;
	upsampleParams.normalsHalfRes = context.normalsTextureHalfRes;
	upsampleParams.renderViewDS   = renderView.GetRenderViewDS();
	return upsampler::DepthBasedUpsample(graphBuilder, aoHalfResTexture, upsampleParams);
}

} // rtao

AmbientOcclusionRenderStage::AmbientOcclusionRenderStage()
	: m_denoiser(RG_DEBUG_NAME("Ambient Occlusion Denoiser"))
{ }

void AmbientOcclusionRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		const RenderView& renderView = viewSpec.GetRenderView();

		rtao::AORenderingContext aoContext{ renderScene, renderView, m_denoiser };
		aoContext.normalsTextureHalfRes      = viewContext.normalsHalfRes;
		aoContext.depthTexture               = viewContext.depth;
		aoContext.depthTextureHalfRes        = viewContext.depthHalfRes;
		aoContext.historyDepthTextureHalfRes = viewContext.historyDepthHalfRes;
		aoContext.motionTextureHalfRes       = viewContext.motionHalfRes;

		viewContext.ambientOcclusion = rtao::RenderAO(graphBuilder, aoContext);
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
