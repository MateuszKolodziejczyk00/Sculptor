#include "RTShadowMaskRenderer.h"
#include "ShaderStructs/ShaderStructs.h"
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
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"
#include "Lights/ViewShadingInput.h"
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"
#include "View/ViewRenderingSpec.h"
#include "RenderScene.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"
#include "Atmosphere/Clouds/VolumetricCloudsTypes.h"
#include "Atmosphere/AtmosphereRenderSystem.h"
#include "Bindless/BindlessTypes.h"
#include "GlobalResources/GlobalResources.h"


namespace spt::rsc
{

namespace params
{

RendererBoolParameter directionalLightEnableShadows("Enable Directional Shadows", { "Lighting", "Shadows", "Directional"}, true);
RendererFloatParameter directionalLightMaxShadowTraceDist("Max Shadow Trace Distance", { "Lighting", "Shadows", "Directional"}, 500.f, 0.f, 200.f);
RendererFloatParameter directionalLightShadowRayBias("Shadow Ray Bias", { "Lighting", "Shadows", "Directional"}, 0.01f, 0.f, 0.1f);

RendererBoolParameter halfResShadows("Half Res", { "Lighting", "Shadows", "Directional" }, false);
RendererBoolParameter enableScreenSpaceShadows("Enable Screen Space Shadows", { "Lighting", "Shadows", "Directional" }, false);
RendererIntParameter screenSpaceShadowsSteps("Screen Space Shadows Steps", { "Lighting", "Shadows", "Directional" }, 8, 1, 64);
RendererFloatParameter screenSpaceShadowsDistance("Screen Space Shadows Distance", { "Lighting", "Shadows", "Directional" }, 0.3f, 0.f, 5.f);
RendererBoolParameter enableScreenSpaceDebug("Enable Screen Space Debug", { "Lighting", "Shadows", "Directional" }, false);

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

namespace screen_space
{

BEGIN_SHADER_STRUCT(ScreenSpaceShadowsConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                                     resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                                     pixelSize)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<Real32>,                       outShadows)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,                       depth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,                       linearDepth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,               normal)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,                       blueNoise256)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint32>,                        inCommandsNum)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<vrt::EncodedRayTraceCommand>,   inCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,                      outRTTraceIndirectArgs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<vrt::EncodedRayTraceCommand>, outRTCommands)
	SHADER_STRUCT_FIELD(Uint32,                                             frameIdx)
	SHADER_STRUCT_FIELD(math::Vector3f,                                     lightDirection)
	SHADER_STRUCT_FIELD(Real32,                                             lightConeAngle)
	SHADER_STRUCT_FIELD(Uint32,                                             stepsNum)
	SHADER_STRUCT_FIELD(Real32,                                             traceDistance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ScreenSpaceShadowsPermutation)
	SHADER_STRUCT_FIELD(rdr::DebugFeature, DEBUG_RAY)
END_SHADER_STRUCT();


COMPUTE_PSO(ScreenSpaceShadowsPSO)
{
	COMPUTE_SHADER("Sculptor/Lights/DirLightsSSShadows.hlsl", TraceSSShadowsCS);

	PRESET(pso);
	PRESET(debug);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso   = CompilePermutation(compiler, ScreenSpaceShadowsPermutation{});
		debug = CompilePermutation(compiler, ScreenSpaceShadowsPermutation{ .DEBUG_RAY = true });
	}
};


static void TraceShadowRays(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ShadowTracingContext& tracingContext, vrt::TracesAllocation& tracesAllocation, rg::RGTextureViewHandle outTexture)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const rg::RGBufferViewHandle rtTraceCommands     = graphBuilder.CreateBufferView(RG_DEBUG_NAME("RT Trace Commands"), tracesAllocation.rayTraceCommands->GetBufferDefinition(), rhi::EMemoryUsage::GPUOnly);
	const rg::RGBufferViewHandle rtTraceIndirectArgs = graphBuilder.CreateBufferView(RG_DEBUG_NAME("RT Trace Indirect Args"), tracesAllocation.tracingIndirectArgs->GetBufferDefinition(), rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Clear RT Indirect Rays Num"), rtTraceIndirectArgs, 0u, sizeof(Uint32), 0u);

	ScreenSpaceShadowsConstants constants;
	constants.resolution             = tracingContext.resolution;
	constants.pixelSize              = tracingContext.resolution.cast<Real32>().cwiseInverse();
	constants.outShadows             = outTexture;
	constants.depth                  = tracingContext.depthTexture;
	constants.linearDepth            = tracingContext.linearDepthTexture;
	constants.normal                 = tracingContext.normalsTexture;
	constants.blueNoise256           = gfx::global::Resources::Get().blueNoise256.GetView();
	constants.inCommandsNum          = tracesAllocation.tracesNum;
	constants.inCommands             = tracesAllocation.rayTraceCommands;
	constants.outRTTraceIndirectArgs = rtTraceIndirectArgs;
	constants.outRTCommands          = rtTraceCommands;
	constants.frameIdx               = renderView.GetRenderedFrameIdx();
	constants.lightDirection         = tracingContext.lightDirection;
	constants.lightConeAngle         = tracingContext.shadowRayConeAngle;
	constants.stepsNum               = params::screenSpaceShadowsSteps;
	constants.traceDistance          = params::screenSpaceShadowsDistance;


	graphBuilder.DispatchIndirect(RG_DEBUG_NAME("SS Shadows"),
								  params::enableScreenSpaceDebug ? ScreenSpaceShadowsPSO::debug : ScreenSpaceShadowsPSO::pso,
								  tracesAllocation.dispatchIndirectArgs, 0u,
								  rg::EmptyDescriptorSets(),
								  constants);

	tracesAllocation.rayTraceCommands     = rtTraceCommands;
	tracesAllocation.tracingIndirectArgs  = rtTraceIndirectArgs;
	tracesAllocation.dispatchIndirectArgs = rg::RGBufferViewHandle();
	tracesAllocation.tracesNum            = rg::RGBufferViewHandle();
}

} // screen_space

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


RT_PSO(ShadowsRayTracingPSO)
{
	RAY_GEN_SHADER("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", GenerateShadowRaysRTG);

	MISS_SHADERS(SHADER_ENTRY("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", GenericRTM));

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/Lights/DirLightsRTShadowsTraceRays.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		pso = CompilePSO(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>());
	}
};


static rg::RGTextureViewHandle TraceShadowRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const ShadowTracingContext& tracingContext)
{
	SPT_PROFILER_FUNCTION();

	vrt::TracesAllocationDefinition tracesAllocationDefinition;
	tracesAllocationDefinition.debugName                        = RG_DEBUG_NAME("Shadows");
	tracesAllocationDefinition.resolution                       = tracingContext.resolution;
	tracesAllocationDefinition.variableRateTexture              = tracingContext.variableRateTexture;
	tracesAllocationDefinition.vrtPermutationSettings           = tracingContext.vrtPermutationSettings;
	tracesAllocationDefinition.outputTracesAndDispatchGroupsNum = params::enableScreenSpaceShadows;
	vrt::TracesAllocation tracesAllocation = AllocateTraces(graphBuilder, tracesAllocationDefinition);

	const rg::RGTextureViewHandle shadowMaskTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Directional Light Shadow Mask"), rg::TextureDef(tracingContext.resolution, rhi::EFragmentFormat::R16_UN_Float));

	if (params::enableScreenSpaceShadows)
	{
		screen_space::TraceShadowRays(graphBuilder, viewSpec, tracingContext, tracesAllocation, shadowMaskTexture);
	}

	const lib::MTHandle<TraceShadowRaysDS> traceShadowRaysDS = graphBuilder.CreateDescriptorSet<TraceShadowRaysDS>(RENDERER_RESOURCE_NAME("Trace Shadow Rays DS"));
	traceShadowRaysDS->u_depthTexture    = tracingContext.depthTexture;
	traceShadowRaysDS->u_normalsTexture  = tracingContext.normalsTexture;
	traceShadowRaysDS->u_traceCommands   = tracesAllocation.rayTraceCommands;

	DirectionalLightShadowUpdateParams updateParams;
	updateParams.lightDirection     = tracingContext.lightDirection;
	updateParams.maxTraceDistance   = params::directionalLightMaxShadowTraceDist;
	updateParams.time               = renderScene.GetCurrentFrameRef().GetTime();
	updateParams.shadowRayConeAngle = tracingContext.shadowRayConeAngle;
	updateParams.enableShadows      = params::directionalLightEnableShadows;
	updateParams.shadowRayBias      = params::directionalLightShadowRayBias;
	updateParams.resolution         = tracingContext.resolution;

	const lib::MTHandle<DirectionalLightShadowMaskDS> directionalLightShadowMaskDS = graphBuilder.CreateDescriptorSet<DirectionalLightShadowMaskDS>(RENDERER_RESOURCE_NAME("Directional Light Shadow Mask DS"));
	directionalLightShadowMaskDS->u_shadowMask = shadowMaskTexture;
	directionalLightShadowMaskDS->u_params     = updateParams;

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("Directional Light Trace Shadow Rays"),
								   ShadowsRayTracingPSO::pso,
								   tracesAllocation.tracingIndirectArgs, 0,
								   rg::BindDescriptorSets(traceShadowRaysDS,
														  directionalLightShadowMaskDS));

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
