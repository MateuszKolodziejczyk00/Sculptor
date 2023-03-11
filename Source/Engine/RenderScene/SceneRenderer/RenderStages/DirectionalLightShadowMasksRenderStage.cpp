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

namespace spt::rsc
{

DS_BEGIN(TraceShadowRaysDS, rg::RGDescriptorSetState<TraceShadowRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),										u_worldAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SceneViewData>),					u_sceneView)
DS_END();


DS_BEGIN(DirectionalLightShadowMaskDS, rg::RGDescriptorSetState<DirectionalLightShadowMaskDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>), u_shadowMask)
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

		const lib::SharedRef<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = rdr::ResourcesManager::CreateDescriptorSetState<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
		directionalLightShadowMaskDS->u_shadowMask = shadowMask.shadowMaskView;

		graphBuilder.TraceRays(RG_DEBUG_NAME("Directional Lights Trace Shadow Rays"),
							   shadowsRayTracingPipeline,
							   math::Vector3u(renderingRes.x(), renderingRes.y(), 1),
							   rg::BindDescriptorSets(traceShadowRaysDS, directionalLightShadowMaskDS));
	}

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
