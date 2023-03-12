#include "DirectionalLightShadowMasksRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "Shadows/ShadowsRenderingTypes.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "Renderer.h"
#include "Common/ShaderCompilationInput.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RayTracing/RayTracingSceneSystem.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "Lights/LightTypes.h"
#include "Shadows/ShadowMapsManagerSystem.h"
#include "Engine.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"

namespace spt::rsc
{

namespace params
{

RendererBoolParameter directionalLightEnableShadows("Enable Shadows", { "Lighting", "Shadows", "Directional"}, true);
RendererFloatParameter directionalLightMinShadowTraceDist("Min Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 0.03f, 0.f, 1.f);
RendererFloatParameter directionalLightMaxShadowTraceDist("Max Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 10.f, 0.f, 100.f);

} // params

BEGIN_SHADER_STRUCT(DirectionalLightShadowUpdateParams)
	SHADER_STRUCT_FIELD(math::Vector3f, lightDirection)
	SHADER_STRUCT_FIELD(Real32,			minTraceDistance)
	SHADER_STRUCT_FIELD(Real32,			maxTraceDistance)
	SHADER_STRUCT_FIELD(Real32,			time)
	SHADER_STRUCT_FIELD(Bool,			enableShadows)
END_SHADER_STRUCT();

DS_BEGIN(TraceShadowRaysDS, rg::RGDescriptorSetState<TraceShadowRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),										u_worldAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SceneViewData>),					u_sceneView)
DS_END();


DS_BEGIN(DirectionalLightShadowMaskDS, rg::RGDescriptorSetState<DirectionalLightShadowMaskDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),											u_shadowMask)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DirectionalLightShadowUpdateParams>),	u_params)
DS_END();


static rdr::PipelineStateID CreateShadowsRayTracingPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateShadowRaysRTG"));
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "ShadowRayRTM"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirectionalLightsShadows.hlsl", compilationSettings);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("Directional Lights Shadows Ray Tracing Pipeline"), shader, pipelineDefinition);
}

DirectionalLightShadowMasksRenderStage::DirectionalLightShadowMasksRenderStage()
{ }

void DirectionalLightShadowMasksRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(rdr::Renderer::IsRayTracingEnabled());

	const RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingResolution();

	DepthPrepassData& depthPrepassData = viewSpec.GetData().Get<DepthPrepassData>();

	const RayTracingSceneSystem* rayTracingSceneSystem = renderScene.GetPrimitivesSystem<RayTracingSceneSystem>();
	SPT_CHECK(!!rayTracingSceneSystem);

	const lib::SharedRef<TraceShadowRaysDS> traceShadowRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<TraceShadowRaysDS>(RENDERER_RESOURCE_NAME("Trace Shadow Rays DS"));
	traceShadowRaysDS->u_worldAccelerationStructure	= lib::Ref(rayTracingSceneSystem->GetSceneTLAS());
	traceShadowRaysDS->u_depthTexture				= depthPrepassData.depth;
	traceShadowRaysDS->u_sceneView					= renderView.GetViewRenderingData();

	const rdr::PipelineStateID shadowsRayTracingPipeline = CreateShadowsRayTracingPipeline();

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();
	const auto directionalLightsView = sceneRegistry.view<DirectionalLightData, DirectionalLightShadowsData>();
	for (const auto& [entity, directionalLight, shadowsData] : directionalLightsView.each())
	{
		const DirectionalLightShadowsData::ShadowMask& shadowMask = shadowsData.GetCurrentFrameShadowMask();

		DirectionalLightShadowUpdateParams params;
		params.lightDirection	= directionalLight.direction;
		params.minTraceDistance = params::directionalLightMinShadowTraceDist;
		params.maxTraceDistance = params::directionalLightMaxShadowTraceDist;
		params.time				= engn::Engine::Get().GetTime();
		params.enableShadows	= params::directionalLightEnableShadows;

		const lib::SharedRef<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = rdr::ResourcesManager::CreateDescriptorSetState<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
		directionalLightShadowMaskDS->u_shadowMask	= shadowMask.shadowMaskView;
		directionalLightShadowMaskDS->u_params		= params;

		graphBuilder.TraceRays(RG_DEBUG_NAME("Directional Lights Trace Shadow Rays"),
							   shadowsRayTracingPipeline,
							   math::Vector3u(renderingRes.x(), renderingRes.y(), 1),
							   rg::BindDescriptorSets(traceShadowRaysDS, directionalLightShadowMaskDS));
	}

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
