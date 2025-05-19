#include "RTShadowMaskRenderer.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
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
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"
#include "View/ViewRenderingSpec.h"
#include "RenderScene.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"
#include "Atmosphere/Clouds/VolumetricCloudsTypes.h"
#include "Atmosphere/AtmosphereRenderSystem.h"


namespace spt::rsc
{

namespace params
{

RendererBoolParameter directionalLightEnableShadows("Enable Directional Shadows", { "Lighting", "Shadows", "Directional"}, true);
RendererFloatParameter directionalLightMaxShadowTraceDist("Max Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 70.f, 0.f, 200.f);
RendererFloatParameter directionalLightShadowRayBias("Shadow Ray Bias", { "Lighting", "Shadows", "Directional"}, 0.01f, 0.f, 0.1f);

RendererBoolParameter halfResShadows("Half Res", { "Lighting", "Shadows", "Directional" }, false);

} // params


struct ShadowTracingContext
{
	math::Vector2u resolution = {};

	math::Vector3f lightDirection = {};
	Real32 shadowRayConeAngle = 0.f;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle linearDepthTexture;
	rg::RGTextureViewHandle normalsTexture;

	rg::RGTextureViewHandle variableRateTexture;

	vrt::VariableRatePermutationSettings vrtPermutationSettings;
};


namespace trace_rays
{

namespace vrt_resolve
{

BEGIN_SHADER_STRUCT(VRTVisibilityResolveConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(VRTVisibilityResolveDS, rg::RGDescriptorSetState<VRTVisibilityResolveDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                           u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                          u_vrBlocksTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_linearDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VRTVisibilityResolveConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateResolveVRTVisibilityPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/VariableRate/Tracing/ResolveVRTVisibility.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ResolveVRTVisibilityCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Resolve VRT Visibility Pipeline"), shader);
}


struct VRTVisibilityResolveParams
{
	rg::RGTextureViewHandle inputTexture;
	rg::RGTextureViewHandle outputTexture;
	rg::RGTextureViewHandle vrBlocksTexture;
	rg::RGTextureViewHandle linearDepthTexture;
	math::Vector2u          resolution;
};


void ResolveVRTVisibility(rg::RenderGraphBuilder& graphBuilder, const VRTVisibilityResolveParams& resolveParams)
{
	SPT_PROFILER_FUNCTION();

	VRTVisibilityResolveConstants constants;
	constants.resolution = resolveParams.resolution;

	const lib::MTHandle<VRTVisibilityResolveDS> resolveDS = graphBuilder.CreateDescriptorSet<VRTVisibilityResolveDS>(RENDERER_RESOURCE_NAME("Resolve VRT Visibility DS"));
	resolveDS->u_inputTexture       = resolveParams.inputTexture;
	resolveDS->u_outputTexture      = resolveParams.outputTexture;
	resolveDS->u_vrBlocksTexture    = resolveParams.vrBlocksTexture;
	resolveDS->u_linearDepthTexture = resolveParams.linearDepthTexture;
	resolveDS->u_constants          = constants;

	static const rdr::PipelineStateID resolvePipeline = CreateResolveVRTVisibilityPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve VRT Visibility"),
						  resolvePipeline,
						  math::Utils::DivideCeil(resolveParams.resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(resolveDS));
}

} // vrt_resolve

BEGIN_SHADER_STRUCT(DirectionalLightShadowUpdateParams)
	SHADER_STRUCT_FIELD(math::Vector3f, lightDirection)
	SHADER_STRUCT_FIELD(Real32,			maxTraceDistance)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(Real32,			shadowRayBias)
	SHADER_STRUCT_FIELD(Real32,			time)
	SHADER_STRUCT_FIELD(Real32,			shadowRayConeAngle)
	SHADER_STRUCT_FIELD(Bool,			enableShadows)
END_SHADER_STRUCT();


DS_BEGIN(TraceShadowRaysDS, rg::RGDescriptorSetState<TraceShadowRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                          u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                  u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>), u_traceCommands)
DS_END();


DS_BEGIN(DirectionalLightShadowMaskDS, rg::RGDescriptorSetState<DirectionalLightShadowMaskDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                u_shadowMask)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DirectionalLightShadowUpdateParams>), u_params)
DS_END();


static rdr::PipelineStateID CreateShadowsRayTracingPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem, const vrt::VariableRatePermutationSettings& variableRatePermutation)
{
	sc::ShaderCompilationSettings compilationSettings;
	vrt::ApplyVariableRatePermutation(INOUT compilationSettings, variableRatePermutation);

	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateShadowRaysRTG"), compilationSettings);
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "RTVisibilityRTM"), compilationSettings));

	const lib::HashedString materialTechnique = "RTVisibility";
	rayTracingSubsystem.FillRayTracingGeometryHitGroups(materialTechnique, INOUT rtShaders.hitGroups);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("Directional Lights Shadows Ray Tracing Pipeline"), rtShaders, pipelineDefinition);
}


static rg::RGTextureViewHandle TraceShadowRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const ShadowTracingContext& tracingContext)
{
	SPT_PROFILER_FUNCTION();

	vrt::TracesAllocationDefinition tracesAllocationDefinition;
	tracesAllocationDefinition.debugName                        = RG_DEBUG_NAME("Shadows");
	tracesAllocationDefinition.resolution                       = tracingContext.resolution;
	tracesAllocationDefinition.variableRateTexture              = tracingContext.variableRateTexture;
	tracesAllocationDefinition.vrtPermutationSettings           = tracingContext.vrtPermutationSettings;
	const vrt::TracesAllocation tracesAllocation = AllocateTraces(graphBuilder, tracesAllocationDefinition);

	const RenderView& renderView = viewSpec.GetRenderView();

	const lib::MTHandle<TraceShadowRaysDS> traceShadowRaysDS = graphBuilder.CreateDescriptorSet<TraceShadowRaysDS>(RENDERER_RESOURCE_NAME("Trace Shadow Rays DS"));
	traceShadowRaysDS->u_depthTexture    = tracingContext.depthTexture;
	traceShadowRaysDS->u_normalsTexture  = tracingContext.normalsTexture;
	traceShadowRaysDS->u_traceCommands   = tracesAllocation.rayTraceCommands;

	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();
	static rdr::PipelineStateID shadowsRayTracingPipeline;
	if (!shadowsRayTracingPipeline.IsValid() || rayTracingSceneSubsystem.AreSBTRecordsDirty())
	{
		shadowsRayTracingPipeline = CreateShadowsRayTracingPipeline(rayTracingSceneSubsystem, tracingContext.vrtPermutationSettings);
	}

	DirectionalLightShadowUpdateParams updateParams;
	updateParams.lightDirection     = tracingContext.lightDirection;
	updateParams.maxTraceDistance   = params::directionalLightMaxShadowTraceDist;
	updateParams.time               = renderScene.GetCurrentFrameRef().GetTime();
	updateParams.shadowRayConeAngle = tracingContext.shadowRayConeAngle;
	updateParams.enableShadows      = params::directionalLightEnableShadows;
	updateParams.shadowRayBias      = params::directionalLightShadowRayBias;
	updateParams.resolution         = tracingContext.resolution;

	const rg::RGTextureViewHandle shadowMaskTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Directional Light Shadow Mask"), rg::TextureDef(tracingContext.resolution, rhi::EFragmentFormat::R16_UN_Float));

	const lib::MTHandle<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = graphBuilder.CreateDescriptorSet<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
	directionalLightShadowMaskDS->u_shadowMask = shadowMaskTexture;
	directionalLightShadowMaskDS->u_params     = updateParams;

	lib::MTHandle<RTVisibilityDS> visibilityDS = graphBuilder.CreateDescriptorSet<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
	visibilityDS->u_geometryDS              = GeometryManager::Get().GetGeometryDSState();
	visibilityDS->u_staticMeshUnifiedDataDS = StaticMeshUnifiedData::Get().GetUnifiedDataDS();
	visibilityDS->u_materialsDS             = mat::MaterialsUnifiedData::Get().GetMaterialsDS();
	visibilityDS->u_sceneRayTracingDS       = rayTracingSceneSubsystem.GetSceneRayTracingDS();

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("Directional Light Trace Shadow Rays"),
								   shadowsRayTracingPipeline,
								   tracesAllocation.tracingIndirectArgs, 0,
								   rg::BindDescriptorSets(traceShadowRaysDS,
														  directionalLightShadowMaskDS,
														  renderView.GetRenderViewDS(),
														  std::move(visibilityDS)));

	const rg::RGTextureViewHandle resolvedShadowMaskTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Directional Light Shadow Mask (Post VRT Resolve)"), rg::TextureDef(tracingContext.resolution, rhi::EFragmentFormat::R16_UN_Float));

	vrt_resolve::VRTVisibilityResolveParams resolveParams;
	resolveParams.inputTexture       = shadowMaskTexture;
	resolveParams.outputTexture      = resolvedShadowMaskTexture;
	resolveParams.vrBlocksTexture    = tracesAllocation.variableRateBlocksTexture;
	resolveParams.linearDepthTexture = tracingContext.linearDepthTexture;
	resolveParams.resolution         = tracingContext.resolution;

	vrt_resolve::ResolveVRTVisibility(graphBuilder, resolveParams);

	return resolvedShadowMaskTexture;

}

} // trace_rays

namespace apply_clouds_shadows
{

BEGIN_SHADER_STRUCT(ApplyCloudsShadowsConstants)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
END_SHADER_STRUCT();


DS_BEGIN(ApplyCloudsShadowsDS, rg::RGDescriptorSetState<ApplyCloudsShadowsDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                    u_shadowMask)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_cloudsTransmittanceMap)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depth)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ApplyCloudsShadowsConstants>),            u_constants)
DS_END();


static rdr::PipelineStateID CreateApplyCloudsShadowsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/ApplyCloudsShadows.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ApplyCloudsShadowsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Apply Clouds Shadows Pipeline"), shader);
}


struct ApplyCloudsShadowsParams
{
	rg::RGTextureViewHandle shadowMask;
	rg::RGTextureViewHandle depth;

	clouds::CloudsTransmittanceMap transmittanceMap;

	lib::MTHandle<RenderViewDS> renderViewDS;
};


void ApplyCloudsShadows(rg::RenderGraphBuilder& graphBuilder, const ApplyCloudsShadowsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.shadowMask->GetResolution2D();

	ApplyCloudsShadowsConstants shaderConstants;
	shaderConstants.viewProjectionMatrix = params.transmittanceMap.viewProjectionMatrix;
	shaderConstants.resolution           = resolution;
	shaderConstants.rcpResolution        = resolution.cast<Real32>().cwiseInverse();

	const lib::MTHandle<ApplyCloudsShadowsDS> ds = graphBuilder.CreateDescriptorSet<ApplyCloudsShadowsDS>(RENDERER_RESOURCE_NAME("ApplyCloudsShadowsDS"));
	ds->u_shadowMask             = params.shadowMask;
	ds->u_cloudsTransmittanceMap = params.transmittanceMap.cloudsTransmittanceTexture;
	ds->u_depth                  = params.depth;
	ds->u_constants              = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateApplyCloudsShadowsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Clouds Shadows"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(ds, params.renderViewDS));
}

} // apply_clouds_shadows

RTShadowMaskRenderer::RTShadowMaskRenderer()
	: m_lightEntity(nullRenderSceneEntity)
	, m_denoiser(RG_DEBUG_NAME("Directional Light Denoiser"))
{
}

void RTShadowMaskRenderer::Initialize(RenderSceneEntity entity)
{
	SPT_CHECK(m_lightEntity == nullRenderSceneEntity);

	m_lightEntity = entity;

	vrt::VariableRateSettings vrtSettings;
	vrtSettings.debugName                           = RG_DEBUG_NAME("RT Shadow Mask");
	vrtSettings.xThreshold2                         = 0.04f;
	vrtSettings.yThreshold2                         = 0.04f;
	vrtSettings.logFramesNumPerSlot                 = 2u;
	vrtSettings.reprojectionFailedMode              = vrt::EReprojectionFailedMode::_1x1;
	vrtSettings.permutationSettings.maxVariableRate = vrt::EMaxVariableRate::_2x2;
	m_variableRateRenderer.Initialize(vrtSettings);
}

void RTShadowMaskRenderer::BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	ShadingViewResourcesUsageInfo& resourcesUsageInfo = viewSpec.GetBlackboard().Get<ShadingViewResourcesUsageInfo>();

	m_renderHalfRes = params::halfResShadows;

	if (m_renderHalfRes)
	{
		resourcesUsageInfo.useLinearDepthHalfRes = true;
	}
	else
	{
		resourcesUsageInfo.useOctahedronNormalsWithHistory = true;
		resourcesUsageInfo.useLinearDepth                  = true;
	}
}

rg::RGTextureViewHandle RTShadowMaskRenderer::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Shadow Mask");

	SPT_CHECK(m_lightEntity != nullRenderSceneEntity);

	const RenderSceneRegistry& sceneRegistry     = renderScene.GetRegistry();
	const DirectionalLightData& directionalLight = sceneRegistry.get<DirectionalLightData>(m_lightEntity);

	const lib::SharedPtr<AtmosphereRenderSystem> atmosphereSystem = renderScene.FindRenderSystem<AtmosphereRenderSystem>();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const rg::RGTextureViewHandle depthTexture        = m_renderHalfRes ? viewContext.depthHalfRes : viewContext.depth;
	const rg::RGTextureViewHandle motionTexture       = m_renderHalfRes ? viewContext.motionHalfRes : viewContext.motion;
	const rg::RGTextureViewHandle linearDepthTexture  = m_renderHalfRes ? viewContext.linearDepthHalfRes : viewContext.linearDepth;
	const rg::RGTextureViewHandle normalsTexture      = m_renderHalfRes ? viewContext.normalsHalfRes : viewContext.octahedronNormals;
	const rg::RGTextureViewHandle historyDepthTexture = m_renderHalfRes ? viewContext.historyDepthHalfRes : viewContext.historyDepth;

	const math::Vector2u shadowMasksRenderingRes = depthTexture->GetResolution2D();

	const vrt::VariableRateReprojectionParams reprojectionParams
	{
		.motionTexture = motionTexture,
	};
	m_variableRateRenderer.Reproject(graphBuilder, reprojectionParams);

	const rg::RGTextureViewHandle variableRateTexture = graphBuilder.AcquireExternalTextureView(m_variableRateRenderer.GetVariableRateTexture());

	ShadowTracingContext tracingContext;
	tracingContext.resolution             = shadowMasksRenderingRes;
	tracingContext.lightDirection         = directionalLight.direction;
	tracingContext.shadowRayConeAngle     = directionalLight.lightConeAngle;
	tracingContext.depthTexture           = depthTexture;
	tracingContext.linearDepthTexture     = linearDepthTexture;
	tracingContext.normalsTexture         = normalsTexture;
	tracingContext.variableRateTexture    = variableRateTexture;
	tracingContext.vrtPermutationSettings = m_variableRateRenderer.GetPermutationSettings();

	const rg::RGTextureViewHandle shadowMaskTexture = trace_rays::TraceShadowRays(graphBuilder, renderScene, viewSpec, tracingContext);

	// Denoise shadow masks
	{
		visibility_denoiser::Denoiser::Params denoiserParams(renderView);
		denoiserParams.historyDepthTexture = historyDepthTexture;
		denoiserParams.currentDepthTexture = depthTexture;
		denoiserParams.motionTexture       = motionTexture;
		denoiserParams.normalsTexture      = normalsTexture;
		m_denoiser.Denoise(graphBuilder, shadowMaskTexture, denoiserParams);
	}

	// Render variable-rate texture

	{
		m_variableRateRenderer.Render(graphBuilder, shadowMaskTexture);
	}

	if (atmosphereSystem && atmosphereSystem->AreVolumetricCloudsEnabled())
	{
		const clouds::CloudsTransmittanceMap& cloudsTransmittanceMap = atmosphereSystem->GetCloudsTransmittanceMap();

		apply_clouds_shadows::ApplyCloudsShadowsParams applyCloudsShadowsParams;
		applyCloudsShadowsParams.shadowMask       = shadowMaskTexture;
		applyCloudsShadowsParams.depth            = viewContext.depth;
		applyCloudsShadowsParams.transmittanceMap = cloudsTransmittanceMap;
		applyCloudsShadowsParams.renderViewDS     = renderView.GetRenderViewDS();
		apply_clouds_shadows::ApplyCloudsShadows(graphBuilder, applyCloudsShadowsParams);
	}

	// Upsample shadow masks

	rg::RGTextureViewHandle shadowMaskFullRes;

	if (m_renderHalfRes)
	{
		upsampler::DepthBasedUpsampleParams upsampleParams;
		upsampleParams.debugName      = RG_DEBUG_NAME("Directional Shadows Upsample");
		upsampleParams.depth          = viewContext.depth;
		upsampleParams.depthHalfRes   = viewContext.depthHalfRes;
		upsampleParams.normalsHalfRes = viewContext.normalsHalfRes;
		upsampleParams.renderViewDS   = renderView.GetRenderViewDS();
		shadowMaskFullRes = upsampler::DepthBasedUpsample(graphBuilder, shadowMaskTexture, upsampleParams);
	}
	else
	{
		shadowMaskFullRes = shadowMaskTexture;
	}

	return shadowMaskFullRes;
}

} // spt::rsc