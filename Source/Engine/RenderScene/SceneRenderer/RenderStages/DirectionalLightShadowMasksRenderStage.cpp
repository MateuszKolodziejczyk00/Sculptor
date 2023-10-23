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
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "Lights/LightTypes.h"
#include "EngineFrame.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"
#include "MaterialsUnifiedData.h"
#include "SceneRenderer/Utils/RTVisibilityUtils.h"
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"
#include "Lights/ViewShadingInput.h"

namespace spt::rsc
{

namespace params
{

RendererBoolParameter directionalLightEnableShadows("Enable Directional Shadows", { "Lighting", "Shadows", "Directional"}, true);
RendererFloatParameter directionalLightMinShadowTraceDist("Min Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 0.03f, 0.f, 1.f);
RendererFloatParameter directionalLightMaxShadowTraceDist("Max Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 30.f, 0.f, 100.f);
RendererFloatParameter directionalLightShadowRayBias("Shadow Ray Bias", { "Lighting", "Shadows", "Directional"}, 0.03f, 0.f, 0.1f);

} // params

BEGIN_SHADER_STRUCT(DirectionalLightShadowUpdateParams)
	SHADER_STRUCT_FIELD(math::Vector3f, lightDirection)
	SHADER_STRUCT_FIELD(Real32,			minTraceDistance)
	SHADER_STRUCT_FIELD(Real32,			maxTraceDistance)
	SHADER_STRUCT_FIELD(Real32,			shadowRayBias)
	SHADER_STRUCT_FIELD(Real32,			time)
	SHADER_STRUCT_FIELD(Real32,			shadowRayConeAngle)
	SHADER_STRUCT_FIELD(Bool,			enableShadows)
END_SHADER_STRUCT();


DS_BEGIN(TraceShadowRaysDS, rg::RGDescriptorSetState<TraceShadowRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),										u_worldAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_geometryNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
DS_END();


DS_BEGIN(DirectionalLightShadowMaskDS, rg::RGDescriptorSetState<DirectionalLightShadowMaskDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),											u_shadowMask)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DirectionalLightShadowUpdateParams>),	u_params)
DS_END();


static rdr::PipelineStateID CreateShadowsRayTracingPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();

	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateShadowRaysRTG"), compilationSettings);
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "RTVIsibilityRTM"), compilationSettings));

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
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("Directional Lights Shadows Ray Tracing Pipeline"), rtShaders, pipelineDefinition);
}

static ViewShadowMasksDataComponent& GetViewShadowMasksDataComponent(RenderSceneEntityHandle viewEntity)
{
	ViewShadowMasksDataComponent* viewShadowMasks = viewEntity.try_get<ViewShadowMasksDataComponent>();
	if (!viewShadowMasks)
	{
		viewShadowMasks = &viewEntity.emplace<ViewShadowMasksDataComponent>();
	}
	return *viewShadowMasks;
}

static ViewShadowMasksDataComponent& UpdateShadowMaskForView(const RenderScene& renderScene, ViewRenderingSpec& viewSpec, math::Vector2u shadowMasksRenderingResolution)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	const RenderSceneEntityHandle viewEntity = renderView.GetViewEntity();

	ViewShadowMasksDataComponent& viewShadowMasks = GetViewShadowMasksDataComponent(viewEntity);

	const rsc::RenderSceneRegistry& registry = renderScene.GetRegistry();

	const auto directionalLights = registry.view<DirectionalLightData>();

	// Remove shadow masks for removed lights
	{
		lib::DynamicArray<RenderSceneEntity> lightsToRemove;
		for (const auto& [entityID, shadowMasks] : viewShadowMasks.directionalLightsShadowData)
		{
			if (!registry.valid(entityID))
			{
				lightsToRemove.emplace_back(entityID);
			}
		}

		for (RenderSceneEntity lightToRemove : lightsToRemove)
		{
			viewShadowMasks.directionalLightsShadowData.erase(lightToRemove);
		}
	}
	return viewShadowMasks;
}

DirectionalLightShadowMasksRenderStage::DirectionalLightShadowMasksRenderStage()
{ }

void DirectionalLightShadowMasksRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(rdr::Renderer::IsRayTracingEnabled());

	const RenderView& renderView = viewSpec.GetRenderView();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
	const MotionData& motionData				= viewSpec.GetData().Get<MotionData>();
	const ShadingInputData& shadingInputData	= viewSpec.GetData().Get<ShadingInputData>();

	const math::Vector2u shadowMasksRenderingRes = depthPrepassData.depthHalfRes->GetResolution2D();

	ViewShadowMasksDataComponent& viewShadowMasks = UpdateShadowMaskForView(renderScene, viewSpec, shadowMasksRenderingRes);
	
	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	lib::HashMap<RenderSceneEntity, rg::RGTextureViewHandle> directionalLightShadowMasks;

	// Do ray tracing to create shadow mask for each directional light

	const lib::MTHandle<TraceShadowRaysDS> traceShadowRaysDS = graphBuilder.CreateDescriptorSet<TraceShadowRaysDS>(RENDERER_RESOURCE_NAME("Trace Shadow Rays DS"));
	traceShadowRaysDS->u_worldAccelerationStructure	= lib::Ref(rayTracingSceneSubsystem.GetSceneTLAS());
	traceShadowRaysDS->u_depthTexture				= depthPrepassData.depthHalfRes;
	traceShadowRaysDS->u_geometryNormalsTexture     = shadingInputData.geometryNormalsHalfRes;

	static rdr::PipelineStateID shadowsRayTracingPipeline;
	if (!shadowsRayTracingPipeline.IsValid() || rayTracingSceneSubsystem.AreSBTRecordsDirty())
	{
		shadowsRayTracingPipeline = CreateShadowsRayTracingPipeline(rayTracingSceneSubsystem);
	}

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();
	const auto directionalLightsView = sceneRegistry.view<DirectionalLightData>();

	// Create shadow mask textures and trace rays

	for (const auto& [entity, directionalLight] : directionalLightsView.each())
	{
		rg::RGTextureViewHandle& shadowMask = directionalLightShadowMasks[entity];
		shadowMask = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Directional Light Shadow Mask"), rg::TextureDef(shadowMasksRenderingRes, rhi::EFragmentFormat::R16_UN_Float));

		DirectionalLightShadowUpdateParams updateParams;
		updateParams.lightDirection		= directionalLight.direction;
		updateParams.minTraceDistance	= params::directionalLightMinShadowTraceDist;
		updateParams.maxTraceDistance	= params::directionalLightMaxShadowTraceDist;
		updateParams.time				= engn::GetRenderingFrame().GetTime();
		updateParams.shadowRayConeAngle = directionalLight.lightConeAngle;
		updateParams.enableShadows		= params::directionalLightEnableShadows;
		updateParams.shadowRayBias		= params::directionalLightShadowRayBias;

		const lib::MTHandle<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = graphBuilder.CreateDescriptorSet<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
		directionalLightShadowMaskDS->u_shadowMask	= shadowMask;
		directionalLightShadowMaskDS->u_params		= updateParams;

		lib::MTHandle<RTVisibilityDS> visibilityDS = graphBuilder.CreateDescriptorSet<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
		visibilityDS->u_rtInstances				= rayTracingSceneSubsystem.GetRTInstancesDataBuffer()->CreateFullView();
		visibilityDS->u_geometryDS				= GeometryManager::Get().GetGeometryDSState();
		visibilityDS->u_staticMeshUnifiedDataDS	= StaticMeshUnifiedData::Get().GetUnifiedDataDS();
		visibilityDS->u_MaterialsDS				= mat::MaterialsUnifiedData::Get().GetMaterialsDS();

		graphBuilder.TraceRays(RG_DEBUG_NAME("Directional Light Trace Shadow Rays"),
							   shadowsRayTracingPipeline,
							   shadowMasksRenderingRes,
							   rg::BindDescriptorSets(traceShadowRaysDS,
													  directionalLightShadowMaskDS,
													  renderView.GetRenderViewDS(),
													  std::move(visibilityDS)));
	}

	// Denoise shadow masks

	for (const auto& [entity, directionalLight] : directionalLightsView.each())
	{
		DirectionalLightRTShadowsData& shadowsData = viewShadowMasks.directionalLightsShadowData[entity];
		const rg::RGTextureViewHandle& shadowMask = directionalLightShadowMasks.at(entity);

		visibility_denoiser::Denoiser::Params denoiserParams(renderView);
		denoiserParams.historyDepthTexture		= depthPrepassData.prevFrameDepthHalfRes;
		denoiserParams.currentDepthTexture		= depthPrepassData.depthHalfRes;
		denoiserParams.motionTexture			= motionData.motionHalfRes;
		denoiserParams.geometryNormalsTexture	= shadingInputData.geometryNormalsHalfRes;

		shadowsData.denoiser.Denoise(graphBuilder, shadowMask, denoiserParams);
	}

	// Upsample shadow masks

	ViewDirectionalShadowMasksData& frameShadowMasks = viewSpec.GetData().Create<ViewDirectionalShadowMasksData>();

	for (const auto& [entity, directionalLight] : directionalLightsView.each())
	{
		const rg::RGTextureViewHandle& shadowMask = directionalLightShadowMasks.at(entity);

		upsampler::DepthBasedUpsampleParams upsampleParams;
		upsampleParams.debugName	= RG_DEBUG_NAME("Directional Shadows Upsample");
		upsampleParams.depth		= depthPrepassData.depth;
		upsampleParams.depthHalfRes = depthPrepassData.depthHalfRes;
		upsampleParams.renderViewDS = renderView.GetRenderViewDS();
		frameShadowMasks.shadowMasks[entity] = upsampler::DepthBasedUpsample(graphBuilder, shadowMask, upsampleParams);
	}

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
