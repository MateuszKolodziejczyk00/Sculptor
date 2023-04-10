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
#include "SceneRenderer/SceneRendererTypes.h"
#include "Lights/LightTypes.h"
#include "EngineFrame.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"

namespace spt::rsc
{

namespace params
{

RendererBoolParameter directionalLightEnableShadows("Enable Directional Shadows", { "Lighting", "Shadows", "Directional"}, true);
RendererFloatParameter directionalLightMinShadowTraceDist("Min Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 0.03f, 0.f, 1.f);
RendererFloatParameter directionalLightMaxShadowTraceDist("Max Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 30.f, 0.f, 100.f);
RendererBoolParameter directionalLightAccumulateShadows("Accumulate Shadows", { "Lighting", "Shadows", "Directional", "Accumulation"}, true);
RendererFloatParameter directionalLightAccumulationMaxDepthDiff("Max Depth Diff", { "Lighting", "Shadows", "Directional", "Accumulation"}, 0.18f, 0.f, 1.f);
RendererFloatParameter directionalLightMinAccumulationAlpha("Min Accumulation Alpha", { "Lighting", "Shadows", "Directional", "Accumulation"}, 0.1f, 0.f, 1.f);
RendererFloatParameter directionalLightMaxAccumulationAlpha("Max Accumulation Alpha", { "Lighting", "Shadows", "Directional", "Accumulation"}, 0.9f, 0.f, 1.f);
RendererBoolParameter directionalLightApplyShadowsBlur("Shadows Blur", { "Lighting", "Shadows", "Directional"}, true);

} // params

BEGIN_SHADER_STRUCT(DirectionalLightShadowUpdateParams)
	SHADER_STRUCT_FIELD(math::Vector3f, lightDirection)
	SHADER_STRUCT_FIELD(Real32,			minTraceDistance)
	SHADER_STRUCT_FIELD(Real32,			maxTraceDistance)
	SHADER_STRUCT_FIELD(Real32,			time)
	SHADER_STRUCT_FIELD(Real32,			shadowRayConeAngle)
	SHADER_STRUCT_FIELD(Bool,			enableShadows)
END_SHADER_STRUCT();

DS_BEGIN(TraceShadowRaysDS, rg::RGDescriptorSetState<TraceShadowRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),										u_worldAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
DS_END();


DS_BEGIN(DirectionalLightShadowMaskDS, rg::RGDescriptorSetState<DirectionalLightShadowMaskDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),											u_shadowMask)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DirectionalLightShadowUpdateParams>),	u_params)
DS_END();


BEGIN_SHADER_STRUCT(DirShadowsAccumulationParams)
	SHADER_STRUCT_FIELD(Real32, maxDepthDifference)
	SHADER_STRUCT_FIELD(Real32, minExponentialAverageAlpha)
	SHADER_STRUCT_FIELD(Real32, maxExponentialAverageAlpha)
END_SHADER_STRUCT();


DS_BEGIN(DirShadowsAccumulationMasksDS, rg::RGDescriptorSetState<DirShadowsAccumulationMasksDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),								u_prevShadowMask)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_shadowMaskSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_shadowMask)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),								u_depth)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),								u_prevDepth)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_prevDepthSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DirShadowsAccumulationParams>),		u_params)
DS_END();


DS_BEGIN(ShadowsBilateralBlurDS, rg::RGDescriptorSetState<ShadowsBilateralBlurDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depth)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_depthSampler)
DS_END()


static rdr::PipelineStateID CreateShadowsRayTracingPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateShadowRaysRTG"));
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "ShadowRayRTM"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirectionalShadows/DirectionalLightsShadows.hlsl", compilationSettings);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("Directional Lights Shadows Ray Tracing Pipeline"), shader, pipelineDefinition);
}

static rdr::PipelineStateID CreateAccumulateShadowsPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AccumulateShadowsCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirectionalShadows/DirectionalLightsShadowsAccumulate.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AccumulateShadowsPipeline"), shader);
}

static rdr::PipelineStateID CreateeShadowsBilateralBlurPipeline(Bool isHorizontal)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ShadowsBilateralBlurCS"));
	if (isHorizontal)
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("IS_HORIZONTAL", "1"));
	}
	else
	{
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("IS_HORIZONTAL", "0"));
	}
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirectionalShadows/ShadowsBilateralBlur.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ShadowsBilateralBlurPipeline"), shader);
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

static ViewShadowMasksDataComponent& UpdateShadowMaskForView(const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	const RenderSceneEntityHandle viewEntity = renderView.GetViewEntity();

	ViewShadowMasksDataComponent& viewShadowMasks = GetViewShadowMasksDataComponent(viewEntity);

	const math::Vector2u viewRenderingRes = renderView.GetRenderingResolution();

	auto& registry = renderScene.GetRegistry();

	const auto directionalLights = registry.view<DirectionalLightData>();

	// Remove shadow masks for removed lights
	{
		lib::DynamicArray<RenderSceneEntity> lightsToRemove;
		for (const auto& [entityID, shadowMasks] : viewShadowMasks.directionalLightShadowMasks)
		{
			if (!registry.valid(entityID))
			{
				lightsToRemove.emplace_back(entityID);
			}
		}

		for (RenderSceneEntity lightToRemove : lightsToRemove)
		{
			viewShadowMasks.directionalLightShadowMasks.erase(lightToRemove);
		}
	}

	// Update shadow masks for existing lights
	{
		for (const auto& [entityID, directionalLightData] : directionalLights.each())
		{
			const RenderSceneEntity lightEntity = RenderSceneEntity(entityID);

			DirectionalLightShadowMasks& shadowMasks = viewShadowMasks.directionalLightShadowMasks[lightEntity];
			shadowMasks.AdvanceFrame();
			lib::SharedPtr<rdr::TextureView>& currentShadowMask = shadowMasks.GetCurrentFrameShadowMask();

			const bool isShadowMaskDirty = !currentShadowMask || currentShadowMask->GetResolution2D() != viewRenderingRes;
			if (isShadowMaskDirty)
			{
				rhi::TextureDefinition shadowMaskDef;
				shadowMaskDef.resolution	= renderView.GetRenderingResolution3D();
				shadowMaskDef.usage			= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
				shadowMaskDef.format		= rhi::EFragmentFormat::R8_UN_Float;
				lib::SharedRef<rdr::Texture> shadowMaskTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask"), shadowMaskDef, rhi::EMemoryUsage::GPUOnly);

				rhi::TextureViewDefinition shadowMaskViewDef;
				shadowMaskViewDef.subresourceRange = rhi::TextureSubresourceRange(spt::rhi::ETextureAspect::Color);
				currentShadowMask = shadowMaskTexture->CreateView(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask View"), shadowMaskViewDef);
			}
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

	ViewShadowMasksDataComponent& viewShadowMasks = UpdateShadowMaskForView(renderScene, viewSpec);

	const RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingResolution();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
	
	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	// Do ray tracing to create shadow mask for each directional light

	const lib::SharedRef<TraceShadowRaysDS> traceShadowRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<TraceShadowRaysDS>(RENDERER_RESOURCE_NAME("Trace Shadow Rays DS"));
	traceShadowRaysDS->u_worldAccelerationStructure	= lib::Ref(rayTracingSceneSubsystem.GetSceneTLAS());
	traceShadowRaysDS->u_depthTexture				= depthPrepassData.depth;

	static const rdr::PipelineStateID shadowsRayTracingPipeline = CreateShadowsRayTracingPipeline();

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();
	const auto directionalLightsView = sceneRegistry.view<DirectionalLightData>();

	for (const auto& [entity, directionalLight] : directionalLightsView.each())
	{
		const DirectionalLightShadowMasks& shadowsData = viewShadowMasks.directionalLightShadowMasks.at(entity);

		const lib::SharedPtr<rdr::TextureView>& shadowMask = shadowsData.GetCurrentFrameShadowMask();

		DirectionalLightShadowUpdateParams updateParams;
		updateParams.lightDirection		= directionalLight.direction;
		updateParams.minTraceDistance	= params::directionalLightMinShadowTraceDist;
		updateParams.maxTraceDistance	= params::directionalLightMaxShadowTraceDist;
		updateParams.time				= engn::GetRenderingFrame().GetTime();
		updateParams.shadowRayConeAngle = directionalLight.lightConeAngle;
		updateParams.enableShadows		= params::directionalLightEnableShadows;

		const lib::SharedRef<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = rdr::ResourcesManager::CreateDescriptorSetState<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
		directionalLightShadowMaskDS->u_shadowMask	= shadowMask;
		directionalLightShadowMaskDS->u_params		= updateParams;

		graphBuilder.TraceRays(RG_DEBUG_NAME("Directional Light Trace Shadow Rays"),
							   shadowsRayTracingPipeline,
							   math::Vector3u(renderingRes.x(), renderingRes.y(), 1),
							   rg::BindDescriptorSets(traceShadowRaysDS, directionalLightShadowMaskDS, renderView.GetRenderViewDS()));
	}

	if (params::directionalLightApplyShadowsBlur)
	{
		// Bilateral blur
		for (const auto& [entity, directionalLight] : directionalLightsView.each())
		{
			const DirectionalLightShadowMasks& shadowsData = viewShadowMasks.directionalLightShadowMasks.at(entity);

			const lib::SharedPtr<rdr::TextureView>& shadowMask		= shadowsData.GetCurrentFrameShadowMask();
			const lib::SharedRef<rdr::Texture>& shadowMaskTexture	= shadowMask->GetTexture();

			rhi::TextureDefinition temporaryTextureDef;
			temporaryTextureDef.resolution	= shadowMaskTexture->GetResolution();
			temporaryTextureDef.usage		= lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
			temporaryTextureDef.format		= shadowMaskTexture->GetRHI().GetDefinition().format;
			const rg::RGTextureViewHandle blurTemporaryTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Directional Shadow Mask Blur Temporary Texture"), temporaryTextureDef, rhi::EMemoryUsage::GPUOnly);

			{

				const lib::SharedRef<ShadowsBilateralBlurDS> shadowsBilateralHorizontalBlurDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowsBilateralBlurDS>(RENDERER_RESOURCE_NAME("Shadows Horizontal Bilateral Blur DS"));
				shadowsBilateralHorizontalBlurDS->u_inputTexture = shadowMask;
				shadowsBilateralHorizontalBlurDS->u_outputTexture = blurTemporaryTexture;
				shadowsBilateralHorizontalBlurDS->u_depth = depthPrepassData.depth;
			
				static const rdr::PipelineStateID shadowsHorizontalBilateralBlurPipeline = CreateeShadowsBilateralBlurPipeline(true);

				const math::Vector3u horizontalBlurDispatchGroups = math::Utils::DivideCeil(math::Vector3u(renderingRes.x(), renderingRes.y(), 1), math::Vector3u(128, 1, 1));

				graphBuilder.Dispatch(RG_DEBUG_NAME("Directional Light Horizontal Bilateral Blur"),
									  shadowsHorizontalBilateralBlurPipeline,
									  horizontalBlurDispatchGroups,
									  rg::BindDescriptorSets(shadowsBilateralHorizontalBlurDS, renderView.GetRenderViewDS()));
			}

			{
				const lib::SharedRef<ShadowsBilateralBlurDS> shadowsBilateralVerticalBlurDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowsBilateralBlurDS>(RENDERER_RESOURCE_NAME("Shadows Vertical Bilateral Blur DS"));
				shadowsBilateralVerticalBlurDS->u_inputTexture = blurTemporaryTexture;
				shadowsBilateralVerticalBlurDS->u_outputTexture = shadowMask;
				shadowsBilateralVerticalBlurDS->u_depth = depthPrepassData.depth;
			
				static const rdr::PipelineStateID shadowsVerticalBilateralBlurPipeline = CreateeShadowsBilateralBlurPipeline(false);

				const math::Vector3u verticalBlurDispatchGroups = math::Utils::DivideCeil(math::Vector3u(renderingRes.y(), renderingRes.x(), 1), math::Vector3u(128, 1, 1));

				graphBuilder.Dispatch(RG_DEBUG_NAME("Directional Light Vertical Bilateral Blur"),
									  shadowsVerticalBilateralBlurPipeline,
									  verticalBlurDispatchGroups,
									  rg::BindDescriptorSets(shadowsBilateralVerticalBlurDS, renderView.GetRenderViewDS()));
			}
		}
	}
	
	// Temporal accumulate shadow data

	if (depthPrepassData.prevFrameDepth.IsValid() && params::directionalLightAccumulateShadows)
	{
		const math::Vector3u accumulationDispatchGroups = math::Utils::DivideCeil(math::Vector3u(renderingRes.x(), renderingRes.y(), 1), math::Vector3u(8, 8, 1));

		static const rdr::PipelineStateID accumulateShadowsPipeline = CreateAccumulateShadowsPipeline();

		for (const auto& [entity, directionalLight] : directionalLightsView.each())
		{
			const DirectionalLightShadowMasks& shadowsData = viewShadowMasks.directionalLightShadowMasks.at(entity);

			const lib::SharedPtr<rdr::TextureView>& prevShadowMask	= shadowsData.GetPreviousFrameShadowMask();
			const lib::SharedPtr<rdr::TextureView>& shadowMask		= shadowsData.GetCurrentFrameShadowMask();

			const Bool canAccumulateShadows = !!prevShadowMask;

			if (canAccumulateShadows)
			{
				DirShadowsAccumulationParams accumulationParams;
				accumulationParams.maxDepthDifference			= params::directionalLightAccumulationMaxDepthDiff;
				accumulationParams.minExponentialAverageAlpha	= params::directionalLightMinAccumulationAlpha;
				accumulationParams.maxExponentialAverageAlpha	= params::directionalLightMaxAccumulationAlpha;

				const lib::SharedRef<DirShadowsAccumulationMasksDS> dirShadowsAccumulationMasksDS = rdr::ResourcesManager::CreateDescriptorSetState<DirShadowsAccumulationMasksDS>(RENDERER_RESOURCE_NAME("Dir Shadows Accumulation Masks DS"));
				dirShadowsAccumulationMasksDS->u_prevShadowMask = prevShadowMask;
				dirShadowsAccumulationMasksDS->u_depth = depthPrepassData.depth;
				dirShadowsAccumulationMasksDS->u_prevDepth = depthPrepassData.prevFrameDepth;
				dirShadowsAccumulationMasksDS->u_shadowMask = shadowMask;
				dirShadowsAccumulationMasksDS->u_params = accumulationParams;

				graphBuilder.Dispatch(RG_DEBUG_NAME("Directional Light Accumulate Shadows"),
									  accumulateShadowsPipeline,
									  accumulationDispatchGroups,
									  rg::BindDescriptorSets(renderView.GetRenderViewDS(), dirShadowsAccumulationMasksDS));
			}
		}
	}

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
