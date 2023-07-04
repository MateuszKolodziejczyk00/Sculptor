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
#include "Denoisers/VisibilityDataDenoiser.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "Utility/Random.h"


namespace spt::rsc
{

namespace rtao
{

struct AORenderingContext
{
	const RenderScene& renderScene;
	const RenderView& renderView;
	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle prevFrameDepthTexture;
	rg::RGTextureViewHandle geometryNormalsTexture;
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


static rdr::PipelineStateID CreateShadowsRayTracingPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();

	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.shaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateAmbientOcclusionRaysRTG"), compilationSettings));
	rtShaders.shaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "AmbientOcclusionRTM"), compilationSettings));

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1u;

	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("RTAO Trace Rays Pipeline"), rtShaders, pipelineDefinition);
}


static rg::RGTextureViewHandle TraceAmbientOcclusionRays(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u renderingResolution = context.renderView.GetRenderingResolution3D();

	const rhi::EFragmentFormat aoTextureFormat = rhi::EFragmentFormat::R16_UN_Float;
	const rhi::TextureDefinition aoTextureDef(renderingResolution, lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture), aoTextureFormat);
	const rg::RGTextureViewHandle aoTextureView = graphBuilder.CreateTextureView(RG_DEBUG_NAME("RTAO Trace Result"), aoTextureDef, rhi::EMemoryUsage::GPUOnly);

	const static rdr::PipelineStateID traceRaysPipeline = CreateShadowsRayTracingPipeline();

	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = context.renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	RTAOTraceRaysParams params;
	params.randomSeed			= math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	params.raysNumber			= 1u;
	params.raysLength			= 0.5f;
	params.raysMinHitDistance	= 0.02f;

	lib::SharedPtr<RTAOTraceRaysDS> traceRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<RTAOTraceRaysDS>(RENDERER_RESOURCE_NAME("RTAOTraceRaysDS"));
	traceRaysDS->u_depthTexture					= context.depthTexture;
	traceRaysDS->u_geometryNormalsTexture		= context.geometryNormalsTexture;
	traceRaysDS->u_ambientOcclusionTexture		= aoTextureView;
	traceRaysDS->u_rtaoParams					= params;
	traceRaysDS->u_worldAccelerationStructure	= lib::Ref(rayTracingSceneSubsystem.GetSceneTLAS());

	graphBuilder.TraceRays(RG_DEBUG_NAME("RTAO Trace Rays"),
						   traceRaysPipeline,
						   renderingResolution,
						   rg::BindDescriptorSets(traceRaysDS, context.renderView.GetRenderViewDS()));

	return aoTextureView;
}


} // trace_rays

static rg::RGTextureViewHandle RenderAO(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle aoTexture = trace_rays::TraceAmbientOcclusionRays(graphBuilder, context);

	// TODO: Denoise

	return aoTexture;
}

} // rtao

AmbientOcclusionRenderStage::AmbientOcclusionRenderStage()
{ }

void AmbientOcclusionRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const DepthPrepassData& depthPrepassData = viewSpec.GetData().Get<DepthPrepassData>();
	SPT_CHECK(depthPrepassData.depth.IsValid());

	ShadingInputData& shadingInputData = viewSpec.GetData().Get<ShadingInputData>();
	SPT_CHECK(shadingInputData.geometryNormals.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();

	rtao::AORenderingContext aoContext{ renderScene, renderView };
	aoContext.geometryNormalsTexture	= shadingInputData.geometryNormals;
	aoContext.depthTexture				= depthPrepassData.depth;
	aoContext.prevFrameDepthTexture		= depthPrepassData.depth;

	const rg::RGTextureViewHandle aoTextureView = rtao::RenderAO(graphBuilder, aoContext);
	shadingInputData.ambientOcclusion = aoTextureView;

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
