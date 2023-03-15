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

RendererBoolParameter directionalLightEnableShadows("Enable Directional Shadows", { "Lighting", "Shadows", "Directional"}, true);
RendererFloatParameter directionalLightMinShadowTraceDist("Min Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 0.03f, 0.f, 1.f);
RendererFloatParameter directionalLightMaxShadowTraceDist("Max Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 30.f, 0.f, 100.f);
RendererBoolParameter directionalLightAccumulateShadows("Accumulate Shadows", { "Lighting", "Shadows", "Directional", "Accumulation"}, true);
RendererFloatParameter directionalLightAccumulationMaxDepthDiff("Max Depth Diff", { "Lighting", "Shadows", "Directional", "Accumulation"}, 0.05f, 0.f, 1.f);
RendererFloatParameter directionalLightMinAccumulationAlpha("Min Accumulation Alpha", { "Lighting", "Shadows", "Directional", "Accumulation"}, 0.1f, 0.f, 1.f);
RendererFloatParameter directionalLightMaxAccumulationAlpha("Max Accumulation Alpha", { "Lighting", "Shadows", "Directional", "Accumulation"}, 0.28f, 0.f, 1.f);
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
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<SceneViewData>),					u_sceneView)
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
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DirShadowsAccumulationParams>),		u_params)
DS_END();


BEGIN_SHADER_STRUCT(ShadowsBilateralBlurParams)
	SHADER_STRUCT_FIELD(Bool,	isHorizontal)
END_SHADER_STRUCT();


DS_BEGIN(ShadowsBilateralBlurDS, rg::RGDescriptorSetState<ShadowsBilateralBlurDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<ShadowsBilateralBlurParams>),		u_params)
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

static rdr::PipelineStateID CreateeShadowsBilateralBlurPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ShadowsBilateralBlurCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirectionalShadows/ShadowsBilateralBlur.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ShadowsBilateralBlurPipeline"), shader);
}


DirectionalLightShadowMasksRenderStage::DirectionalLightShadowMasksRenderStage()
{ }

void DirectionalLightShadowMasksRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(rdr::Renderer::IsRayTracingEnabled());

	const RenderView& renderView = viewSpec.GetRenderView();
	const math::Vector2u renderingRes = renderView.GetRenderingResolution();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
	
	const RayTracingSceneSystem* rayTracingSceneSystem = renderScene.GetPrimitivesSystem<RayTracingSceneSystem>();
	SPT_CHECK(!!rayTracingSceneSystem);

	// Do ray tracing to create shadow mask for each directional light

	const lib::SharedRef<TraceShadowRaysDS> traceShadowRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<TraceShadowRaysDS>(RENDERER_RESOURCE_NAME("Trace Shadow Rays DS"));
	traceShadowRaysDS->u_worldAccelerationStructure	= lib::Ref(rayTracingSceneSystem->GetSceneTLAS());
	traceShadowRaysDS->u_depthTexture				= depthPrepassData.depth;
	traceShadowRaysDS->u_sceneView					= renderView.GetViewRenderingData();

	static const rdr::PipelineStateID shadowsRayTracingPipeline = CreateShadowsRayTracingPipeline();

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();
	const auto directionalLightsView = sceneRegistry.view<DirectionalLightData, DirectionalLightShadowsData>();

	for (const auto& [entity, directionalLight, shadowsData] : directionalLightsView.each())
	{
		const DirectionalLightShadowsData::ShadowMask& shadowMask = shadowsData.GetCurrentFrameShadowMask();

		DirectionalLightShadowUpdateParams updateParams;
		updateParams.lightDirection		= directionalLight.direction;
		updateParams.minTraceDistance	= params::directionalLightMinShadowTraceDist;
		updateParams.maxTraceDistance	= params::directionalLightMaxShadowTraceDist;
		updateParams.time				= engn::Engine::Get().GetTime();
		updateParams.shadowRayConeAngle = directionalLight.lightConeAngle;
		updateParams.enableShadows		= params::directionalLightEnableShadows;

		const lib::SharedRef<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = rdr::ResourcesManager::CreateDescriptorSetState<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
		directionalLightShadowMaskDS->u_shadowMask	= shadowMask.shadowMaskView;
		directionalLightShadowMaskDS->u_params		= updateParams;

		graphBuilder.TraceRays(RG_DEBUG_NAME("Directional Light Trace Shadow Rays"),
							   shadowsRayTracingPipeline,
							   math::Vector3u(renderingRes.x(), renderingRes.y(), 1),
							   rg::BindDescriptorSets(traceShadowRaysDS, directionalLightShadowMaskDS));
	}

	const math::Vector3u postProcessDispatchGroups = math::Utils::DivideCeil(math::Vector3u(renderingRes.x(), renderingRes.y(), 1), math::Vector3u(8, 8, 1));
	
	// Temporal accumulate shadow data

	if (depthPrepassData.prevFrameDepth.IsValid() && params::directionalLightAccumulateShadows)
	{
		static const rdr::PipelineStateID accumulateShadowsPipeline = CreateAccumulateShadowsPipeline();

		for (const auto& [entity, directionalLight, shadowsData] : directionalLightsView.each())
		{
			const DirectionalLightShadowsData::ShadowMask& prevShadowMask = shadowsData.GetPreviousFrameShadowMask();
			const DirectionalLightShadowsData::ShadowMask& shadowMask = shadowsData.GetCurrentFrameShadowMask();

			const Bool canAccumulateShadows = !!prevShadowMask.shadowMaskView;

			if (canAccumulateShadows)
			{
				DirShadowsAccumulationParams accumulationParams;
				accumulationParams.maxDepthDifference			= params::directionalLightAccumulationMaxDepthDiff;
				accumulationParams.minExponentialAverageAlpha	= params::directionalLightMinAccumulationAlpha;
				accumulationParams.maxExponentialAverageAlpha	= params::directionalLightMaxAccumulationAlpha;

				const lib::SharedRef<DirShadowsAccumulationMasksDS> dirShadowsAccumulationMasksDS = rdr::ResourcesManager::CreateDescriptorSetState<DirShadowsAccumulationMasksDS>(RENDERER_RESOURCE_NAME("Dir Shadows Accumulation Masks DS"));
				dirShadowsAccumulationMasksDS->u_prevShadowMask = prevShadowMask.shadowMaskView;
				dirShadowsAccumulationMasksDS->u_depth = depthPrepassData.depth;
				dirShadowsAccumulationMasksDS->u_prevDepth = depthPrepassData.prevFrameDepth;
				dirShadowsAccumulationMasksDS->u_shadowMask = shadowMask.shadowMaskView;
				dirShadowsAccumulationMasksDS->u_params = accumulationParams;

				graphBuilder.Dispatch(RG_DEBUG_NAME("Directional Light Accumulate Shadows"),
									  accumulateShadowsPipeline,
									  postProcessDispatchGroups,
									  rg::BindDescriptorSets(renderView.GetRenderViewDSRef(), dirShadowsAccumulationMasksDS));
			}
		}
	}

	if (params::directionalLightApplyShadowsBlur)
	{
		static const rdr::PipelineStateID shadowsBilateralBlurPipeline = CreateeShadowsBilateralBlurPipeline();

		// Bilateral blur
		for (const auto& [entity, directionalLight, shadowsData] : directionalLightsView.each())
		{
			const DirectionalLightShadowsData::ShadowMask& prevShadowMask = shadowsData.GetPreviousFrameShadowMask();
			const DirectionalLightShadowsData::ShadowMask& shadowMask = shadowsData.GetCurrentFrameShadowMask();

			rg::RGTextureViewHandle blurTemporaryTexture;
			if (!!prevShadowMask.shadowMaskView && prevShadowMask.shadowMaskView->GetTexture()->GetResolution() == shadowMask.shadowMaskView->GetTexture()->GetResolution())
			{
				blurTemporaryTexture = graphBuilder.AcquireExternalTextureView(prevShadowMask.shadowMaskView);
			}
			else
			{
				const lib::SharedRef<rdr::Texture>& shadowMaskTexture = shadowMask.shadowMaskView->GetTexture();

				rhi::TextureDefinition temporaryTextureDef;
				temporaryTextureDef.resolution = shadowMaskTexture->GetResolution();
				temporaryTextureDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture);
				temporaryTextureDef.format = shadowMaskTexture->GetRHI().GetDefinition().format;
				blurTemporaryTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Directional Shadow Mask Blur Temporary Texture"), temporaryTextureDef, rhi::EMemoryUsage::GPUOnly);
			}


			{
				ShadowsBilateralBlurParams horizontalBlurParams;
				horizontalBlurParams.isHorizontal = true;

				const lib::SharedRef<ShadowsBilateralBlurDS> shadowsBilateralHorizontalBlurDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowsBilateralBlurDS>(RENDERER_RESOURCE_NAME("Shadows Horizontal Bilateral Blur DS"));
				shadowsBilateralHorizontalBlurDS->u_inputTexture = shadowMask.shadowMaskView;
				shadowsBilateralHorizontalBlurDS->u_outputTexture = blurTemporaryTexture;
				shadowsBilateralHorizontalBlurDS->u_params = horizontalBlurParams;
				shadowsBilateralHorizontalBlurDS->u_depth = depthPrepassData.depth;

				graphBuilder.Dispatch(RG_DEBUG_NAME("Directional Light Horizontal Bilateral Blur"),
									  shadowsBilateralBlurPipeline,
									  postProcessDispatchGroups,
									  rg::BindDescriptorSets(shadowsBilateralHorizontalBlurDS, renderView.GetRenderViewDSRef()));
			}

			{
				ShadowsBilateralBlurParams verticalBlurParams;
				verticalBlurParams.isHorizontal = false;

				const lib::SharedRef<ShadowsBilateralBlurDS> shadowsBilateralVerticalBlurDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowsBilateralBlurDS>(RENDERER_RESOURCE_NAME("Shadows Vertical Bilateral Blur DS"));
				shadowsBilateralVerticalBlurDS->u_inputTexture = blurTemporaryTexture;
				shadowsBilateralVerticalBlurDS->u_outputTexture = shadowMask.shadowMaskView;
				shadowsBilateralVerticalBlurDS->u_params = verticalBlurParams;
				shadowsBilateralVerticalBlurDS->u_depth = depthPrepassData.depth;

				graphBuilder.Dispatch(RG_DEBUG_NAME("Directional Light Vertical Bilateral Blur"),
									  shadowsBilateralBlurPipeline,
									  postProcessDispatchGroups,
									  rg::BindDescriptorSets(shadowsBilateralVerticalBlurDS, renderView.GetRenderViewDSRef()));
			}
		}
	}

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
