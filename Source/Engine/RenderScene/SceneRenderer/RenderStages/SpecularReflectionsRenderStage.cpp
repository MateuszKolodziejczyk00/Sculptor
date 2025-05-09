#include "SpecularReflectionsRenderStage.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ChildDSBinding.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"
#include "MaterialsUnifiedData.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "Lights/LightsRenderSystem.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "DDGI/DDGISceneSubsystem.h"
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "SceneRenderer/RenderStages/Utils/RayBinner.h"
#include "SceneRenderer/Utils/RTVisibilityUtils.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "SceneRenderer/RenderStages/Utils/RTReflectionsTypes.h"
#include "SceneRenderer/Debug/Stats/RTReflectionsStatsView.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::SpecularReflections, SpecularReflectionsRenderStage);

namespace renderer_params
{
RendererBoolParameter enableTemporalResampling("Enable Temporal Resampling", { "Specular Reflections" }, true);
RendererIntParameter spatialResamplingIterationsNum("Spatial Resampling Iterations Num", { "Specular Reflections" }, 1, 0, 2);

RendererBoolParameter halfResReflections("Half Res", { "Specular Reflections" }, false);
RendererBoolParameter forceFullRateTracingReflections("Force Full Rate Tracing", { "Specular Reflections" }, false);
RendererBoolParameter doFullFinalVisibilityCheck("Full Final Visibility Check", { "Specular Reflections" }, true);
RendererBoolParameter enableSecondTracingPass("Enable SecondTracing Pass", { "Specular Reflections" }, false);
RendererBoolParameter blurVarianceEstimate("Blur Variance Estimate", { "Specular Reflections" }, true);
RendererBoolParameter useDepthTestForVRTReprojection("Use Depth Test For VRT Reprojection", { "Specular Reflections" }, false);
RendererBoolParameter enableSpatialResamplingSSVisibilityTest("Enable Spatial Resampling SS Visibility Test", { "Specular Reflections" }, false);
RendererBoolParameter useLargeTileSize("Use Large Tile Size", { "Specular Reflections" }, false);
RendererBoolParameter resampleOnlyFromTracedPixels("Resample Only From Traced Pixels", { "Specular Reflections" }, true);
RendererBoolParameter enableHitDistanceBasedMaxAge("Enable Hit Distance Based Max Age", { "Specular Reflections" }, true);
RendererIntParameter reservoirMaxAge("Reservoir Max Age", { "Specular Reflections" }, 10, 0, 30);
RendererFloatParameter resamplingRangeStep("Resampling Range Step", { "Specular Reflections" }, 2.333f, 0.f, 10.f);
RendererBoolParameter enableStableHistoryBlend("Enable Stable History Blend", { "Specular Reflections" }, true);
RendererBoolParameter enableDisocclusionFixFromLightCache("Enable Disocclusion Fix From Light Cache", { "Specular Reflections" }, true);
} // renderer_params

struct SpecularReflectionsParams
{
	math::Vector2u resolution;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle depthHistoryTexture;
	rg::RGTextureViewHandle linearDepthTexture;
	rg::RGTextureViewHandle motionTexture;
	rg::RGTextureViewHandle normalsTexture;
	rg::RGTextureViewHandle historyNormalsTexture;
	rg::RGTextureViewHandle roughnessTexture;
	rg::RGTextureViewHandle historyRoughnessTexture;
	rg::RGTextureViewHandle baseColorTexture;
	rg::RGTextureViewHandle skyViewLUT;
};


namespace trace
{

namespace ray_directions
{

BEGIN_SHADER_STRUCT(GenerateRayDirectionsConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Uint32,         seed)
END_SHADER_STRUCT()


DS_BEGIN(GenerateRayDirectionsDS, rg::RGDescriptorSetState<GenerateRayDirectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                       u_tracesNum)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>),  u_traceCommands)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                   u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                   u_baseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                           u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                           u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                     u_rwRaysDirections)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Real32>),                     u_rwRaysPdfs)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GenerateRayDirectionsConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateGenerateRayDirectionsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/GenerateRayDirections.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateRayDirectionsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateRayDirectionsPipeline"), shader);
}


struct RayDirections
{
	rg::RGBufferViewHandle rayDirections;
	rg::RGBufferViewHandle rayPdfs;
};


RayDirections GenerateRayDirections(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params, const vrt::TracesAllocation& tracesAllocation)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u resolution = params.resolution;

	rhi::BufferDefinition rayDirectionsBufferDef;
	rayDirectionsBufferDef.size  = resolution.x() * resolution.y() * sizeof(Uint32);
	rayDirectionsBufferDef.usage = rhi::EBufferUsage::Storage;
	const rg::RGBufferViewHandle rayDirectionsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Ray Directions Buffer"), rayDirectionsBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition rayPdfsBufferDef;
	rayPdfsBufferDef.size  = resolution.x() * resolution.y() * sizeof(Real32);
	rayPdfsBufferDef.usage = rhi::EBufferUsage::Storage;
	const rg::RGBufferViewHandle rayPdfsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Ray PDFs Buffer"), rayPdfsBufferDef, rhi::EMemoryUsage::GPUOnly);

	GenerateRayDirectionsConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.seed          = lib::rnd::RandomFromTypeDomain<Uint32>();

	lib::MTHandle<GenerateRayDirectionsDS> generateRayDirectionsDS = graphBuilder.CreateDescriptorSet<GenerateRayDirectionsDS>(RENDERER_RESOURCE_NAME("GenerateRayDirectionsDS"));
	generateRayDirectionsDS->u_tracesNum                = tracesAllocation.tracesNum;
	generateRayDirectionsDS->u_traceCommands            = tracesAllocation.rayTraceCommands;
	generateRayDirectionsDS->u_normalsTexture           = params.normalsTexture;
	generateRayDirectionsDS->u_baseColorMetallicTexture = params.baseColorTexture;
	generateRayDirectionsDS->u_depthTexture             = params.depthTexture;
	generateRayDirectionsDS->u_roughnessTexture         = params.roughnessTexture;
	generateRayDirectionsDS->u_rwRaysDirections         = rayDirectionsBuffer;
	generateRayDirectionsDS->u_rwRaysPdfs               = rayPdfsBuffer;
	generateRayDirectionsDS->u_constants                = shaderConstants;

	const rdr::PipelineStateID generateRayDirectionsPipeline = CreateGenerateRayDirectionsPipeline();

	graphBuilder.DispatchIndirect(RG_DEBUG_NAME("Generate Ray Directions"),
								  generateRayDirectionsPipeline,
								  tracesAllocation.dispatchIndirectArgs, 0,
								  rg::BindDescriptorSets(std::move(generateRayDirectionsDS), renderView.GetRenderViewDS()));

	return RayDirections{ rayDirectionsBuffer, rayPdfsBuffer };
}

} // ray_directions

namespace shading
{

struct RTGBuffer
{
	rg::RGBufferViewHandle hitMaterialInfos;

	rg::RGBufferViewHandle rayDirections;
	rg::RGBufferViewHandle rayPdfs;
};


struct RTShadingParams
{
	RTGBuffer rtGBuffer;

	vrt::TracesAllocation tracesAllocation;

	rg::RGBufferViewHandle sortedTraces;
	rg::RGBufferViewHandle shadingIndirectArgs;
	rg::RGBufferViewHandle raysShadingCounts;

	math::Vector2u         reservoirsResolution;
	rg::RGBufferViewHandle reservoirsBuffer;
};


BEGIN_SHADER_STRUCT(RTShadingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector2u, reservoirsResolution)
	SHADER_STRUCT_FIELD(Uint32,         rayCommandsBufferSize)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RTHitMaterialInfo)
	SHADER_STRUCT_FIELD(Uint32, normal)
	SHADER_STRUCT_FIELD(Uint32, baseColorMetallic)
	SHADER_STRUCT_FIELD(Uint16, roughness)
	SHADER_STRUCT_FIELD(Real32, hitDistance)
	SHADER_STRUCT_FIELD(Uint32, emissive)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RaysShadingIndirectArgs)
	SHADER_STRUCT_FIELD(math::Vector3u, hitDispatchSize)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(math::Vector3u, missDispatchGroups)
	SHADER_STRUCT_FIELD(Uint32, padding1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RaysShadingCounts)
	SHADER_STRUCT_FIELD(Uint32, hitRaysNum)
	SHADER_STRUCT_FIELD(Uint32, missRaysNum)
END_SHADER_STRUCT();


static constexpr Uint64 ComputeHitShadingIndirectArgsOffset()
{
	return offsetof(RaysShadingIndirectArgs, hitDispatchSize);
}


static constexpr Uint64 ComputeMissShadingIndirectArgsOffset()
{
	return offsetof(RaysShadingIndirectArgs, missDispatchGroups);
}


DS_BEGIN(RTShadingDS, rg::RGDescriptorSetState<RTShadingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                               u_rayDirections)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Real32>),                               u_rayPdfs)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTHitMaterialInfo>),                    u_hitMaterialInfos)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<sr_restir::SRPackedReservoir>),       u_reservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RaysShadingCounts>),                    u_tracesNum)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>),          u_traceCommands)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                               u_sortedTraces)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RTShadingConstants>),                     u_constants)
DS_END();

namespace miss_rays
{

static rdr::PipelineStateID CreateMissRaysShadingPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/MissRaysShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "MissRaysShadingCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("MissRaysShadingPipeline"), shader);
}


static void ShadeMissRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, const RTShadingParams& shadingParams, lib::MTHandle<RTShadingDS> shadingDS)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	static const rdr::PipelineStateID missRaysShadingPipeline = CreateMissRaysShadingPipeline();

	graphBuilder.DispatchIndirect(RG_DEBUG_NAME("Miss Rays Shading"),
								  missRaysShadingPipeline,
								  shadingParams.shadingIndirectArgs, ComputeMissShadingIndirectArgsOffset(),
								  rg::BindDescriptorSets(std::move(shadingDS), renderView.GetRenderViewDS()));
}

} // miss_rays


namespace hit_rays
{

static rdr::PipelineStateID CreateHitRaysShadingPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/HitRaysShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "HitRaysShadingRTG"));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/HitRaysShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "ShadowRayRTM")));

	const lib::HashedString materialTechnique = "RTVisibility";
	rayTracingSubsystem.FillRayTracingGeometryHitGroups(materialTechnique, INOUT rtShaders.hitGroups);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("Trace Specular Reflections Rays Pipeline"), rtShaders, pipelineDefinition);
}


static void ShadeHitRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, const RTShadingParams& shadingParams, lib::MTHandle<RTShadingDS> shadingDS)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const LightsRenderSystem& lightsRenderSystem        = renderScene.GetRenderSystemChecked<LightsRenderSystem>();
	const ShadowMapsManagerSubsystem& shadowMapsManager = renderScene.GetSceneSubsystemChecked<ShadowMapsManagerSubsystem>();

	const ddgi::DDGISceneSubsystem& ddgiSceneSubsystem = renderScene.GetSceneSubsystemChecked<ddgi::DDGISceneSubsystem>();
	lib::MTHandle<ddgi::DDGISceneDS> ddgiDS = ddgiSceneSubsystem.GetDDGISceneDS();

	const RayTracingRenderSceneSubsystem& rayTracingSceneSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	static rdr::PipelineStateID hitRaysShadingPipeline;
	if (!hitRaysShadingPipeline.IsValid() || rayTracingSceneSubsystem.AreSBTRecordsDirty())
	{
		hitRaysShadingPipeline = CreateHitRaysShadingPipeline(rayTracingSceneSubsystem);
	}

	lib::MTHandle<RTVisibilityDS> visibilityDS = graphBuilder.CreateDescriptorSet<RTVisibilityDS>(RENDERER_RESOURCE_NAME("RT Visibility DS"));
	visibilityDS->u_geometryDS              = GeometryManager::Get().GetGeometryDSState();
	visibilityDS->u_staticMeshUnifiedDataDS = StaticMeshUnifiedData::Get().GetUnifiedDataDS();
	visibilityDS->u_materialsDS             = mat::MaterialsUnifiedData::Get().GetMaterialsDS();
	visibilityDS->u_sceneRayTracingDS       = rayTracingSceneSubsystem.GetSceneRayTracingDS();

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("Hit Rays Shading"),
								   hitRaysShadingPipeline,
								   shadingParams.shadingIndirectArgs, ComputeHitShadingIndirectArgsOffset(),
								   rg::BindDescriptorSets(std::move(shadingDS),
														  renderView.GetRenderViewDS(),
														  std::move(ddgiDS),
														  lightsRenderSystem.GetGlobalLightsDS(),
														  shadowMapsManager.GetShadowMapsDS(),
														  visibilityDS));
}

} // hit_rays

static void ShadeRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, const RTShadingParams& shadingParams)
{
	SPT_PROFILER_FUNCTION();

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& atmosphereContext          = atmosphereSubsystem.GetAtmosphereContext();

	RTShadingConstants rtShadingConstants;
	rtShadingConstants.resolution            = srParams.resolution;
	rtShadingConstants.invResolution         = srParams.resolution.cast<Real32>().cwiseInverse();
	rtShadingConstants.reservoirsResolution  = shadingParams.reservoirsResolution;
	rtShadingConstants.rayCommandsBufferSize = static_cast<Uint32>(shadingParams.sortedTraces->GetSize() / sizeof(vrt::EncodedRayTraceCommand));

	const lib::MTHandle<RTShadingDS> shadingDS = graphBuilder.CreateDescriptorSet<RTShadingDS>(RENDERER_RESOURCE_NAME("RTShadingDS"));
	shadingDS->u_skyViewLUT       = srParams.skyViewLUT;
	shadingDS->u_transmittanceLUT = atmosphereContext.transmittanceLUT;
	shadingDS->u_atmosphereParams = atmosphereContext.atmosphereParamsBuffer->CreateFullView();
	shadingDS->u_hitMaterialInfos = shadingParams.rtGBuffer.hitMaterialInfos;
	shadingDS->u_rayDirections    = shadingParams.rtGBuffer.rayDirections;
	shadingDS->u_rayPdfs          = shadingParams.rtGBuffer.rayPdfs;
	shadingDS->u_depthTexture     = srParams.depthTexture;
	shadingDS->u_reservoirsBuffer = shadingParams.reservoirsBuffer;
	shadingDS->u_traceCommands    = shadingParams.tracesAllocation.rayTraceCommands;
	shadingDS->u_sortedTraces     = shadingParams.sortedTraces;
	shadingDS->u_tracesNum        = shadingParams.raysShadingCounts;
	shadingDS->u_constants        = rtShadingConstants;

	miss_rays::ShadeMissRays(graphBuilder, renderScene, viewSpec, srParams, shadingParams, shadingDS);
	hit_rays::ShadeHitRays(graphBuilder, renderScene, viewSpec, srParams, shadingParams, shadingDS);
}

} // shading

namespace vrt_resolve
{

BEGIN_SHADER_STRUCT(RTHitMaterialInfo)
	SHADER_STRUCT_FIELD(Uint32, normal)
	SHADER_STRUCT_FIELD(Uint32, baseColorMetallic)
	SHADER_STRUCT_FIELD(Uint16, roughness)
	SHADER_STRUCT_FIELD(Real32, hitDistance)
END_SHADER_STRUCT();


DS_BEGIN(RTShadingDS, rg::RGDescriptorSetState<RTShadingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_rayDirectionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_rayPdfTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTHitMaterialInfo>),                    u_hitMaterialInfos)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<sr_restir::SRPackedReservoir>),         u_reservoirsBuffer)
DS_END();

} // vrt_resolve


BEGIN_SHADER_STRUCT(SpecularReflectionsTraceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Uint32,         rayCommandsBufferSize)
END_SHADER_STRUCT();


DS_BEGIN(SpecularReflectionsTraceDS, rg::RGDescriptorSetState<SpecularReflectionsTraceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<SceneRayTracingDS>),                             rayTracingDS)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                               u_rayDirections)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>),          u_traceCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<shading::RTHitMaterialInfo>),         u_hitMaterialInfos)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                             u_sortedRays) // hits first
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<shading::RaysShadingIndirectArgs>),   u_shadingIndirectArgs)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<shading::RaysShadingCounts>),         u_raysShadingCounts)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SpecularReflectionsTraceConstants>),      u_constants)
DS_END();


static rdr::PipelineStateID CreateSpecularReflectionsTracePipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/SpecularReflectionsTrace.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateSpecularReflectionsRaysRTG"));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/SpecularReflectionsTrace.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "SpecularReflectionsRTM")));

	const lib::HashedString materialTechnique = "SpecularReflections";
	rayTracingSubsystem.FillRayTracingGeometryHitGroups(materialTechnique, INOUT rtShaders.hitGroups);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("Trace Specular Reflections Rays Pipeline"), rtShaders, pipelineDefinition);
}


struct TraceParams
{
	vrt::TracesAllocation tracesAllocation;

	math::Vector2u         reservoirsResolution;
	rg::RGBufferViewHandle reservoirsBuffer;
};


static void GenerateReservoirs(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params, const TraceParams& traceParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, lib::String("Generate Reservoirs ") + debugName.AsString());

	const RenderView& renderView = viewSpec.GetRenderView();

	// Phase (1) Generate Ray Directions

	const auto [rayDirections, rayPdfs] = ray_directions::GenerateRayDirections(graphBuilder, renderScene, viewSpec, params, traceParams.tracesAllocation);

	// Phase (2) Trace Rays

	RayTracingRenderSceneSubsystem& rayTracingSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	static rdr::PipelineStateID traceRaysPipeline;
	if (!traceRaysPipeline.IsValid() || rayTracingSubsystem.AreSBTRecordsDirty())
	{
		traceRaysPipeline = CreateSpecularReflectionsTracePipeline(rayTracingSubsystem);
	}

	const Uint32 maxTracesNum = params.resolution.x() * params.resolution.y();
	const Uint64 hitMaterialInfosBufferSize = maxTracesNum * sizeof(shading::RTHitMaterialInfo);

	rhi::BufferDefinition hitMaterialInfosBufferDef;
	hitMaterialInfosBufferDef.size  = hitMaterialInfosBufferSize;
	hitMaterialInfosBufferDef.usage = rhi::EBufferUsage::Storage;
	const rg::RGBufferViewHandle hitMaterialInfos = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Hit Material Infos Buffer"), hitMaterialInfosBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition sortedRaysBufferDef;
	sortedRaysBufferDef.size  = traceParams.tracesAllocation.rayTraceCommands->GetSize();
	sortedRaysBufferDef.usage = rhi::EBufferUsage::Storage;
	const rg::RGBufferViewHandle sortedRaysBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Sorted Rays Buffer"), sortedRaysBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition shadingIndirectArgsBufferDef;
	shadingIndirectArgsBufferDef.size  = sizeof(shading::RaysShadingIndirectArgs);
	shadingIndirectArgsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::DeviceAddress);
	const rg::RGBufferViewHandle shadingIndirectArgsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Shading Indirect Args Buffer"), shadingIndirectArgsBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition raysShadingCountsBufferDef;
	raysShadingCountsBufferDef.size  = sizeof(shading::RaysShadingCounts);
	raysShadingCountsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle raysShadingCountsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Rays Shading Counts Buffer"), raysShadingCountsBufferDef, rhi::EMemoryUsage::GPUOnly);

	SpecularReflectionsTraceConstants shaderConstants;
	shaderConstants.resolution    = params.resolution;
	shaderConstants.invResolution = params.resolution.cast<Real32>().cwiseInverse();
	shaderConstants.rayCommandsBufferSize = static_cast<Uint32>(sortedRaysBuffer->GetSize() / sizeof(vrt::EncodedRayTraceCommand));

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Shading Indirect Args"), shadingIndirectArgsBuffer, 0u);
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Rays Shading Counts"), raysShadingCountsBuffer, 0u);

	lib::MTHandle<SpecularReflectionsTraceDS> traceRaysDS = graphBuilder.CreateDescriptorSet<SpecularReflectionsTraceDS>(RENDERER_RESOURCE_NAME("SpecularReflectionsTraceDS"));
	traceRaysDS->rayTracingDS          = rayTracingSubsystem.GetSceneRayTracingDS();
	traceRaysDS->u_depthTexture        = params.depthTexture;
	traceRaysDS->u_rayDirections       = rayDirections;
	traceRaysDS->u_traceCommands       = traceParams.tracesAllocation.rayTraceCommands;
	traceRaysDS->u_hitMaterialInfos    = hitMaterialInfos;
	traceRaysDS->u_sortedRays          = sortedRaysBuffer;
	traceRaysDS->u_shadingIndirectArgs = shadingIndirectArgsBuffer;
	traceRaysDS->u_raysShadingCounts   = raysShadingCountsBuffer;
	traceRaysDS->u_constants           = shaderConstants;

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("Specular Reflections Trace Rays"),
								   traceRaysPipeline,
								   traceParams.tracesAllocation.tracingIndirectArgs, 0u,
								   rg::BindDescriptorSets(std::move(traceRaysDS),
														  renderView.GetRenderViewDS(),
														  mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
														  GeometryManager::Get().GetGeometryDSState(),
														  StaticMeshUnifiedData::Get().GetUnifiedDataDS()));

	// Phase (3) Shade Rays

	shading::RTGBuffer rtGBuffer;
	rtGBuffer.hitMaterialInfos = hitMaterialInfos;
	rtGBuffer.rayDirections    = rayDirections;
	rtGBuffer.rayPdfs          = rayPdfs;

	shading::RTShadingParams shadingParams;
	shadingParams.rtGBuffer            = rtGBuffer;
	shadingParams.tracesAllocation     = traceParams.tracesAllocation;
	shadingParams.sortedTraces         = sortedRaysBuffer;
	shadingParams.shadingIndirectArgs  = shadingIndirectArgsBuffer;
	shadingParams.raysShadingCounts    = raysShadingCountsBuffer;
	shadingParams.reservoirsResolution = traceParams.reservoirsResolution;
	shadingParams.reservoirsBuffer     = traceParams.reservoirsBuffer;

	shading::ShadeRays(graphBuilder, renderScene, viewSpec, params, shadingParams);
}

} // trace

namespace denoise
{

static sr_denoiser::Denoiser::Result Denoise(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, sr_denoiser::Denoiser& denoiser, const SpecularReflectionsParams& params, rg::RGTextureViewHandle specularReflections, rg::RGTextureViewHandle diffuseReflections)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	sr_denoiser::Denoiser::Params denoiserParams(renderView);
	denoiserParams.currentDepthTexture                 = params.depthTexture;
	denoiserParams.historyDepthTexture                 = params.depthHistoryTexture;
	denoiserParams.linearDepthTexture                  = params.linearDepthTexture;
	denoiserParams.motionTexture                       = params.motionTexture;
	denoiserParams.normalsTexture                      = params.normalsTexture;
	denoiserParams.historyNormalsTexture               = params.historyNormalsTexture;
	denoiserParams.roughnessTexture                    = params.roughnessTexture;
	denoiserParams.historyRoughnessTexture             = params.historyRoughnessTexture;
	denoiserParams.specularTexture                     = specularReflections;
	denoiserParams.diffuseTexture                      = diffuseReflections;
	denoiserParams.blurVarianceEstimate                = renderer_params::blurVarianceEstimate;
	denoiserParams.enableStableHistoryBlend            = renderer_params::enableStableHistoryBlend;
	denoiserParams.enableDisocclusionFixFromLightCache = renderer_params::enableDisocclusionFixFromLightCache;

	return denoiser.Denoise(graphBuilder, denoiserParams);
}

} // denoise

namespace vrt
{

static rdr::ShaderID CreateVariableRateTextureShader(const VariableRatePermutationSettings& permutationSettings)
{
	sc::ShaderCompilationSettings compilationSettings;
	ApplyVariableRatePermutation(INOUT compilationSettings, permutationSettings);
	return rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/RTCreateVariableRateTexture.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CreateVariableRateTextureCS"), compilationSettings);
}


BEGIN_SHADER_STRUCT(VariableRateRTConstants)
	SHADER_STRUCT_FIELD(Uint32, forceFullRateTracing)
END_SHADER_STRUCT();


DS_BEGIN(RTVariableRateTextureDS, rg::RGDescriptorSetState<RTVariableRateTextureDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_influenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_varianceEstimation)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_linearDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_specularReflectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_diffuseReflectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VariableRateRTConstants>), u_rtConstants)
DS_END();

} // vrt

#if SPT_ENABLE_SCENE_RENDERER_STATS
namespace stats
{

void RecordStatsSample(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution, const vrt::TracesAllocation& firstPassTracesAllocation, const vrt::TracesAllocation& secondPassTracesAllocation)
{
	SPT_PROFILER_FUNCTION();

	lib::SharedRef<rdr::Buffer> firstPassTracesNum = graphBuilder.DownloadBuffer(RG_DEBUG_NAME("Download First Pass Traces Num"), firstPassTracesAllocation.tracesNum, 0, 4u);
	lib::SharedPtr<rdr::Buffer> secondPassTracesNum;

	if (secondPassTracesAllocation.IsValid())
	{
		secondPassTracesNum = graphBuilder.DownloadBuffer(RG_DEBUG_NAME("Download Second Pass Traces Num"), secondPassTracesAllocation.tracesNum, 0, 4u);
	}

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [firstPassTracesNum, secondPassTracesNum, resolution]
			   {
				   const Uint32 firstPassTracesNumValue = firstPassTracesNum->Map<Uint32>()[0];

				   const Uint32 secondPassTracesNumValue = secondPassTracesNum ? secondPassTracesNum->Map<Uint32>()[0] : 0u;

				   RTReflectionsStatsView& statsView = SceneRendererStatsRegistry::GetInstance().GetStatsView<RTReflectionsStatsView>();
				   
				   RTReflectionsStatsView::FrameSample sample;
				   sample.resolution        = resolution;
				   sample.numRaysFirstPass  = firstPassTracesNumValue;
				   sample.numRaysSecondPass = secondPassTracesNumValue;

				   statsView.RecordFrameSample(std::move(sample));
			   },
			   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
}

} // stats
#endif // SPT_ENABLE_SCENE_RENDERER_STATS


SpecularReflectionsRenderStage::SpecularReflectionsRenderStage()
	: m_denoiser(RG_DEBUG_NAME("RT Denoiser"))
{
	vrt::VariableRateSettings variableRateSettings;
	variableRateSettings.debugName   = RG_DEBUG_NAME("RT Reflections");
	variableRateSettings.logFramesNumPerSlot    = 0u;
	variableRateSettings.reprojectionFailedMode = vrt::EReprojectionFailedMode::_1x2;
	variableRateSettings.permutationSettings.maxVariableRate = vrt::EMaxVariableRate::_4x4;
	variableRateSettings.permutationSettings.useLargeTile    = true;
	m_variableRateRenderer.Initialize(variableRateSettings);
}

void SpecularReflectionsRenderStage::BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	Base::BeginFrame(renderScene, viewSpec);

	ShadingViewResourcesUsageInfo& resourcesUsageInfo = viewSpec.GetBlackboard().Get<ShadingViewResourcesUsageInfo>();

	m_renderHalfResReflections = renderer_params::halfResReflections;

	if (m_renderHalfResReflections)
	{
		resourcesUsageInfo.useHalfResRoughnessWithHistory = true;
		resourcesUsageInfo.useHalfResBaseColorWithHistory = true;
		resourcesUsageInfo.useLinearDepthHalfRes          = true;
	}
	else
	{
		resourcesUsageInfo.useRoughnessHistory             = true;
		resourcesUsageInfo.useBaseColorHistory             = true;
		resourcesUsageInfo.useOctahedronNormalsWithHistory = true;
		resourcesUsageInfo.useLinearDepth                  = true;
	}

	if (m_variableRateRenderer.GetVariableRateSettings().permutationSettings.useLargeTile != renderer_params::useLargeTileSize)
	{
		vrt::VariableRateSettings newVariableRateSettings = m_variableRateRenderer.GetVariableRateSettings();
		newVariableRateSettings.permutationSettings.useLargeTile = renderer_params::useLargeTileSize;

		m_variableRateRenderer.Reinitialize(newVariableRateSettings);
	}
}

void SpecularReflectionsRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RT Reflections");

		const RenderView& renderView = viewSpec.GetRenderView();

		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		const ShadingViewRenderingSystemsInfo& viewSystemsInfo = viewSpec.GetBlackboard().Get<ShadingViewRenderingSystemsInfo>();

		const Bool isHalfRes = m_renderHalfResReflections;

		const math::Vector2u resolution = isHalfRes ? viewSpec.GetRenderingHalfRes() : viewSpec.GetRenderingRes();

		const rg::RGTextureViewHandle motionTexture       = isHalfRes ? viewContext.motionHalfRes : viewContext.motion;
		const rg::RGTextureViewHandle historyDepthTexture = isHalfRes ? viewContext.historyDepthHalfRes : viewContext.historyDepth;

		const rg::RGTextureViewHandle vrReprojectionSuccessMask = vrt::CreateReprojectionSuccessMask(graphBuilder, resolution, m_variableRateRenderer.GetVariableRateSettings());

		vrt::VariableRateReprojectionParams reprojectionParams
		{
			.motionTexture           = motionTexture,
			.reprojectionSuccessMask = vrReprojectionSuccessMask,
		};

		if (renderer_params::useDepthTestForVRTReprojection)
		{
			reprojectionParams.geometryData = vrt::VariableRateReprojectionParams::Geometry
			{
				.currentDepth   = isHalfRes ? viewContext.depthHalfRes : viewContext.depth,
				.currentNormals = isHalfRes ? viewContext.octahedronNormals : viewContext.normalsHalfRes,
				.historyDepth   = historyDepthTexture,
				.renderView     = &renderView
			};
		}

		m_variableRateRenderer.Reproject(graphBuilder, reprojectionParams);

		const Bool hasValidHistory = historyDepthTexture.IsValid() && historyDepthTexture->GetResolution2D() == resolution;

		vrt::TracesAllocationDefinition tracesAllocationDefinition;
		tracesAllocationDefinition.debugName                        = RG_DEBUG_NAME("Specular Reflections");
		tracesAllocationDefinition.resolution                       = resolution;
		tracesAllocationDefinition.variableRateTexture              = graphBuilder.AcquireExternalTextureView(m_variableRateRenderer.GetVariableRateTexture());
		tracesAllocationDefinition.vrtPermutationSettings           = m_variableRateRenderer.GetPermutationSettings();
		tracesAllocationDefinition.outputTracesAndDispatchGroupsNum = true;
		tracesAllocationDefinition.traceIdx                         = renderView.GetRenderedFrameIdx();
		const vrt::TracesAllocation tracesAllocation = AllocateTraces(graphBuilder, tracesAllocationDefinition);

		SpecularReflectionsParams params;
		params.resolution = resolution;
		params.skyViewLUT = viewContext.skyViewLUT;

		if (isHalfRes)
		{
			params.normalsTexture          = viewContext.normalsHalfRes;
			params.historyNormalsTexture   = viewContext.historyNormalsHalfRes;
			params.roughnessTexture        = viewContext.roughnessHalfRes;
			params.historyRoughnessTexture = viewContext.historyRoughnessHalfRes;
			params.baseColorTexture        = viewContext.baseColorHalfRes;
			params.depthTexture            = viewContext.depthHalfRes;
			params.depthHistoryTexture     = viewContext.historyDepthHalfRes;
			params.linearDepthTexture      = viewContext.linearDepthHalfRes;
			params.motionTexture           = viewContext.motionHalfRes;
		}
		else
		{
			params.normalsTexture          = viewContext.octahedronNormals;
			params.historyNormalsTexture   = viewContext.historyOctahedronNormals;
			params.roughnessTexture        = viewContext.gBuffer[GBuffer::Texture::Roughness];
			params.historyRoughnessTexture = viewContext.historyRoughness;
			params.baseColorTexture        = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
			params.depthTexture            = viewContext.depth;
			params.depthHistoryTexture     = viewContext.historyDepth;
			params.linearDepthTexture      = viewContext.linearDepth;
			params.motionTexture           = viewContext.motion;
		}

		const math::Vector2u reservoirsResolution = sr_restir::ComputeReservoirsResolution(resolution);
		const rg::RGBufferViewHandle reservoirsBuffer = sr_restir::CreateReservoirsBuffer(graphBuilder, reservoirsResolution);

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize reservoirs"), reservoirsBuffer, 0u);

		trace::TraceParams traceParams;
		traceParams.tracesAllocation     = tracesAllocation;
		traceParams.reservoirsResolution = reservoirsResolution;
		traceParams.reservoirsBuffer     = reservoirsBuffer;

		trace::GenerateReservoirs(graphBuilder, RG_DEBUG_NAME("1st Pass"), renderScene, viewSpec, params, traceParams);

		const rg::RGTextureViewHandle specularLuminanceHitDistanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Luminance Hit Distance Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));
		const rg::RGTextureViewHandle diffuseLuminanceHitDistanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Diffuse Hit Distance Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));

		const lib::StaticArray<sr_restir::SpatialResamplingPassParams, 2u> spatialResamplingPasses =
		{
			sr_restir::SpatialResamplingPassParams{
				.resamplingRangeMultiplier        = 1.f,
				.samplesNum                       = 3u,
				.enableScreenSpaceVisibilityTrace = renderer_params::enableSpatialResamplingSSVisibilityTest,
				.resampleOnlyFromTracedPixels     = renderer_params::resampleOnlyFromTracedPixels
			},
			sr_restir::SpatialResamplingPassParams{
				.resamplingRangeMultiplier        = 0.2f,
				.samplesNum                       = 2u,
				.enableScreenSpaceVisibilityTrace = renderer_params::enableSpatialResamplingSSVisibilityTest,
				.resampleOnlyFromTracedPixels     = renderer_params::resampleOnlyFromTracedPixels
			}
		};

		const SizeType activeResamplingPassesNum = std::min(spatialResamplingPasses.size(), static_cast<SizeType>(renderer_params::spatialResamplingIterationsNum));
		const lib::Span<const sr_restir::SpatialResamplingPassParams> activeResamplingPasses = { spatialResamplingPasses.data(), activeResamplingPassesNum };

		sr_restir::ResamplingParams resamplingParams(renderView, renderScene);
		resamplingParams.initialReservoirBuffer          = reservoirsBuffer;
		resamplingParams.depthTexture                    = params.depthTexture;
		resamplingParams.normalsTexture                  = params.normalsTexture;
		resamplingParams.roughnessTexture                = params.roughnessTexture;
		resamplingParams.baseColorTexture                = params.baseColorTexture;
		resamplingParams.historyDepthTexture             = params.depthHistoryTexture;
		resamplingParams.historyNormalsTexture           = params.historyNormalsTexture;
		resamplingParams.historyRoughnessTexture         = params.historyRoughnessTexture;
		resamplingParams.historyBaseColorTexture         = isHalfRes ? viewContext.historyBaseColorHalfRes : viewContext.historyBaseColor;
		resamplingParams.outSpecularLuminanceDistTexture = specularLuminanceHitDistanceTexture;
		resamplingParams.outDiffuseLuminanceDistTexture  = diffuseLuminanceHitDistanceTexture;
		resamplingParams.motionTexture                   = params.motionTexture;
		resamplingParams.historySpecularHitDist          = m_denoiser.GetHistorySpecular(graphBuilder);
		resamplingParams.tracesAllocation                = tracesAllocation;
		resamplingParams.vrReprojectionSuccessMask       = vrReprojectionSuccessMask;
		resamplingParams.enableTemporalResampling        = hasValidHistory && renderer_params::enableTemporalResampling;
		resamplingParams.spatialResamplingPasses         = activeResamplingPasses;
		resamplingParams.doFullFinalVisibilityCheck      = renderer_params::doFullFinalVisibilityCheck;
		resamplingParams.enableSecondTracingPass         = renderer_params::enableSecondTracingPass;
		resamplingParams.variableRateTileSizeBitOffset   = vrt::GetTileSizeBitOffset(m_variableRateRenderer.GetVariableRateSettings());
		resamplingParams.enableHitDistanceBasedMaxAge    = renderer_params::enableHitDistanceBasedMaxAge;
		resamplingParams.reservoirMaxAge                 = renderer_params::reservoirMaxAge;
		resamplingParams.resamplingRangeStep             = renderer_params::resamplingRangeStep;

		const sr_restir::InitialResamplingResult initialResamplingResult = m_resampler.ExecuteInitialResampling(graphBuilder, resamplingParams);

		const vrt::TracesAllocation& additionalTracesAllocation = initialResamplingResult.additionalTracesAllocation;
		if (additionalTracesAllocation.IsValid())
		{
			trace::TraceParams additionalTracesParams;
			additionalTracesParams.tracesAllocation     = additionalTracesAllocation;
			additionalTracesParams.reservoirsResolution = reservoirsResolution;
			additionalTracesParams.reservoirsBuffer     = initialResamplingResult.resampledReservoirsBuffer;

			trace::GenerateReservoirs(graphBuilder, RG_DEBUG_NAME("2nd Pass"), renderScene, viewSpec, params, additionalTracesParams);
		}

		m_resampler.ExecuteFinalResampling(graphBuilder, resamplingParams, initialResamplingResult);

		rg::RGTextureViewHandle specularReflectionsFullRes;
		rg::RGTextureViewHandle diffuseReflectionsFullRes;
		rg::RGTextureViewHandle varianceEstimation;

		if (viewSystemsInfo.useUnifiedDenoising)
		{
			if (isHalfRes)
			{
				SPT_CHECK_NO_ENTRY();
			}
			else
			{
				specularReflectionsFullRes = specularLuminanceHitDistanceTexture;
				diffuseReflectionsFullRes  = diffuseLuminanceHitDistanceTexture;
			}

			//TODO: Temporary, doesn't matter in practice
			varianceEstimation = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Variance Estimation"), rg::TextureDef(resolution, rhi::EFragmentFormat::RG16_S_Float));
		}
		else
		{
			const sr_denoiser::Denoiser::Result denoiserResult = denoise::Denoise(graphBuilder,
																				  renderScene,
																				  viewSpec,
																				  m_denoiser,
																				  params,
																				  specularLuminanceHitDistanceTexture,
																				  diffuseLuminanceHitDistanceTexture);

			if (isHalfRes)
			{
				{
					upsampler::DepthBasedUpsampleParams specularUpsampleParams;
					specularUpsampleParams.debugName               = RG_DEBUG_NAME("Upsample Specular Reflections");
					specularUpsampleParams.depth                   = viewContext.depth;
					specularUpsampleParams.depthHalfRes            = viewContext.depthHalfRes;
					specularUpsampleParams.normalsHalfRes          = viewContext.normalsHalfRes;
					specularUpsampleParams.renderViewDS            = renderView.GetRenderViewDS();
					specularUpsampleParams.fireflyFilteringEnabled = true;
					specularReflectionsFullRes = upsampler::DepthBasedUpsample(graphBuilder, denoiserResult.denoisedSpecular, specularUpsampleParams);
				}

				{
					upsampler::DepthBasedUpsampleParams diffuseUpsampleParams;
					diffuseUpsampleParams.debugName               = RG_DEBUG_NAME("Upsample Diffuse Reflections");
					diffuseUpsampleParams.depth                   = viewContext.depth;
					diffuseUpsampleParams.depthHalfRes            = viewContext.depthHalfRes;
					diffuseUpsampleParams.normalsHalfRes          = viewContext.normalsHalfRes;
					diffuseUpsampleParams.renderViewDS            = renderView.GetRenderViewDS();
					diffuseUpsampleParams.fireflyFilteringEnabled = true;
					diffuseReflectionsFullRes = upsampler::DepthBasedUpsample(graphBuilder, denoiserResult.denoisedDiffuse, diffuseUpsampleParams);
				}
			}
			else
			{
				specularReflectionsFullRes = denoiserResult.denoisedSpecular;
				diffuseReflectionsFullRes  = denoiserResult.denoisedDiffuse;
			}

			varianceEstimation = denoiserResult.varianceEstimation;
		}

		SPT_CHECK(specularReflectionsFullRes.IsValid())
		SPT_CHECK(diffuseReflectionsFullRes.IsValid())

		const rg::RGTextureViewHandle reflectionsInfluenceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reflections Influence Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::RG16_S_Float));

		RTReflectionsViewData reflectionsViewData;
		reflectionsViewData.finalDiffuseGI  = diffuseReflectionsFullRes;
		reflectionsViewData.finalSpecularGI = specularReflectionsFullRes;

		reflectionsViewData.reflectionsInfluenceTexture  = reflectionsInfluenceTexture;
		reflectionsViewData.varianceEstimation           = varianceEstimation;
		reflectionsViewData.halfResReflections           = isHalfRes;

		viewSpec.GetBlackboard().Create<RTReflectionsViewData>(reflectionsViewData);

		viewSpec.GetRenderStageEntries(ERenderStage::CompositeLighting).GetPostRenderStage().AddRawMember(this, &SpecularReflectionsRenderStage::RenderVariableRateTexture);

#if SPT_ENABLE_SCENE_RENDERER_STATS
		stats::RecordStatsSample(graphBuilder, resolution, tracesAllocation, additionalTracesAllocation);
#endif // SPT_ENABLE_SCENE_RENDERER_STATS
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

void SpecularReflectionsRenderStage::RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const RTReflectionsViewData& reflectionsViewData = viewSpec.GetBlackboard().Get<RTReflectionsViewData>();

	const Bool isHalfRes = reflectionsViewData.halfResReflections;

	const rdr::ShaderID vrtShader = vrt::CreateVariableRateTextureShader(m_variableRateRenderer.GetPermutationSettings());

	vrt::VariableRateRTConstants shaderConstants;
	shaderConstants.forceFullRateTracing = renderer_params::forceFullRateTracingReflections;

	lib::MTHandle<vrt::RTVariableRateTextureDS> vrtDS = graphBuilder.CreateDescriptorSet<vrt::RTVariableRateTextureDS>(RENDERER_RESOURCE_NAME("RTVariableRateTextureDS"));
	vrtDS->u_influenceTexture           = reflectionsViewData.reflectionsInfluenceTexture;
	vrtDS->u_roughnessTexture           = isHalfRes ? viewContext.roughnessHalfRes : viewContext.gBuffer[GBuffer::Texture::Roughness];
	vrtDS->u_varianceEstimation         = reflectionsViewData.varianceEstimation;
	vrtDS->u_linearDepthTexture         = isHalfRes ? viewContext.linearDepthHalfRes : viewContext.linearDepth;
	vrtDS->u_specularReflectionsTexture = reflectionsViewData.finalSpecularGI;
	vrtDS->u_diffuseReflectionsTexture  = reflectionsViewData.finalDiffuseGI;
	vrtDS->u_rtConstants                = shaderConstants;
	m_variableRateRenderer.Render(graphBuilder, nullptr, vrtShader, rg::BindDescriptorSets(vrtDS));
}

} // spt::rsc
