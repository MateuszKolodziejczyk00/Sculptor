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


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::SpecularReflections, SpecularReflectionsRenderStage);

namespace renderer_params
{
RendererFloatParameter specularTrace2x2Threshold("Specular Trace 2x2 Threshold", { "Specular Reflections" }, 0.055f, 0.f, 0.1f);
RendererFloatParameter specularTrace4x4Threshold("Specular Trace 4x4 Threshold", { "Specular Reflections" }, 0.032f, 0.f, 0.1f);
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
	rg::RGTextureViewHandle specularColorTexture;
	rg::RGTextureViewHandle skyViewLUT;
};


namespace trace
{

namespace ray_directions
{

BEGIN_SHADER_STRUCT(GenerateRayDirectionsConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, random)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT()


DS_BEGIN(GenerateRayDirectionsDS, rg::RGDescriptorSetState<GenerateRayDirectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                       u_tracesNum)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>),  u_traceCommands)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                   u_normalsTexture)
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
	shaderConstants.random        = math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx      = renderView.GetRenderedFrameIdx();

	lib::MTHandle<GenerateRayDirectionsDS> generateRayDirectionsDS = graphBuilder.CreateDescriptorSet<GenerateRayDirectionsDS>(RENDERER_RESOURCE_NAME("GenerateRayDirectionsDS"));
	generateRayDirectionsDS->u_tracesNum        = tracesAllocation.tracesNum;
	generateRayDirectionsDS->u_traceCommands    = tracesAllocation.rayTraceCommands;
	generateRayDirectionsDS->u_normalsTexture   = params.normalsTexture;
	generateRayDirectionsDS->u_depthTexture     = params.depthTexture;
	generateRayDirectionsDS->u_roughnessTexture = params.roughnessTexture;
	generateRayDirectionsDS->u_rwRaysDirections = rayDirectionsBuffer;
	generateRayDirectionsDS->u_rwRaysPdfs       = rayPdfsBuffer;
	generateRayDirectionsDS->u_constants        = shaderConstants;

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

	math::Vector2u         reservoirsResolution;
	rg::RGBufferViewHandle reservoirsBuffer;
};


BEGIN_SHADER_STRUCT(RTShadingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector2u, reservoirsResolution)
END_SHADER_STRUCT();


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
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                               u_rayDirections)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Real32>),                               u_rayPdfs)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTHitMaterialInfo>),                    u_hitMaterialInfos)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<sr_restir::SRPackedReservoir>),       u_reservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                               u_tracesNum)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>),          u_traceCommands)
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
								  shadingParams.tracesAllocation.dispatchIndirectArgs, 0,
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
								   shadingParams.tracesAllocation.tracingIndirectArgs, 0,
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
	rtShadingConstants.resolution           = srParams.resolution;
	rtShadingConstants.invResolution        = srParams.resolution.cast<Real32>().cwiseInverse();
	rtShadingConstants.reservoirsResolution = shadingParams.reservoirsResolution;

	const lib::MTHandle<RTShadingDS> shadingDS = graphBuilder.CreateDescriptorSet<RTShadingDS>(RENDERER_RESOURCE_NAME("RTShadingDS"));
	shadingDS->u_skyViewLUT                  = srParams.skyViewLUT;
	shadingDS->u_transmittanceLUT            = atmosphereContext.transmittanceLUT;
	shadingDS->u_atmosphereParams            = atmosphereContext.atmosphereParamsBuffer->CreateFullView();
	shadingDS->u_hitMaterialInfos            = shadingParams.rtGBuffer.hitMaterialInfos;
	shadingDS->u_rayDirections               = shadingParams.rtGBuffer.rayDirections;
	shadingDS->u_rayPdfs                     = shadingParams.rtGBuffer.rayPdfs;
	shadingDS->u_depthTexture                = srParams.depthTexture;
	shadingDS->u_reservoirsBuffer            = shadingParams.reservoirsBuffer;
	shadingDS->u_traceCommands               = shadingParams.tracesAllocation.rayTraceCommands;
	shadingDS->u_tracesNum                   = shadingParams.tracesAllocation.tracesNum;
	shadingDS->u_constants                   = rtShadingConstants;

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
END_SHADER_STRUCT();


DS_BEGIN(SpecularReflectionsTraceDS, rg::RGDescriptorSetState<SpecularReflectionsTraceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<SceneRayTracingDS>),                             rayTracingDS)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),                               u_rayDirections)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<shading::RTHitMaterialInfo>),         u_hitMaterialInfos)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<vrt::EncodedRayTraceCommand>),          u_traceCommands)
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


static void GenerateReservoirs(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params, const TraceParams& traceParams)
{
	SPT_PROFILER_FUNCTION();

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

	SpecularReflectionsTraceConstants shaderConstants;
	shaderConstants.resolution    = params.resolution;
	shaderConstants.invResolution = params.resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<SpecularReflectionsTraceDS> traceRaysDS = graphBuilder.CreateDescriptorSet<SpecularReflectionsTraceDS>(RENDERER_RESOURCE_NAME("SpecularReflectionsTraceDS"));
	traceRaysDS->rayTracingDS       = rayTracingSubsystem.GetSceneRayTracingDS();
	traceRaysDS->u_depthTexture     = params.depthTexture;
	traceRaysDS->u_rayDirections    = rayDirections;
	traceRaysDS->u_hitMaterialInfos = hitMaterialInfos;
	traceRaysDS->u_traceCommands    = traceParams.tracesAllocation.rayTraceCommands;
	traceRaysDS->u_constants        = shaderConstants;

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
	shadingParams.reservoirsResolution = traceParams.reservoirsResolution;
	shadingParams.reservoirsBuffer     = traceParams.reservoirsBuffer;

	shading::ShadeRays(graphBuilder, renderScene, viewSpec, params, shadingParams);
}

} // trace

namespace denoise
{

static rg::RGTextureViewHandle Denoise(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, sr_denoiser::Denoiser& denoiser, const SpecularReflectionsParams& params, rg::RGTextureViewHandle denoisedTexture)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	sr_denoiser::Denoiser::Params denoiserParams(renderView);
	denoiserParams.currentDepthTexture       = params.depthTexture;
	denoiserParams.historyDepthTexture       = params.depthHistoryTexture;
	denoiserParams.linearDepthTexture        = params.linearDepthTexture;
	denoiserParams.motionTexture             = params.motionTexture;
	denoiserParams.normalsTexture            = params.normalsTexture;
	denoiserParams.historyNormalsTexture     = params.historyNormalsTexture;
	denoiserParams.roughnessTexture          = params.roughnessTexture;
	denoiserParams.historyRoughnessTexture   = params.historyRoughnessTexture;
	return denoiser.Denoise(graphBuilder, denoisedTexture, denoiserParams);
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

DS_BEGIN(RTVariableRateTextureDS, rg::RGDescriptorSetState<RTVariableRateTextureDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>), u_influenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>), u_roughnessTexture)
DS_END();

} // vrt


SpecularReflectionsRenderStage::SpecularReflectionsRenderStage()
	: m_denoiser(RG_DEBUG_NAME("SR Denoiser"))
{
	vrt::VariableRateSettings variableRateSettings;
	variableRateSettings.xThreshold2 = renderer_params::specularTrace2x2Threshold;
	variableRateSettings.yThreshold2 = renderer_params::specularTrace2x2Threshold;
	variableRateSettings.xThreshold4 = renderer_params::specularTrace4x4Threshold;
	variableRateSettings.yThreshold4 = renderer_params::specularTrace4x4Threshold;
	variableRateSettings.logFramesNumPerSlot    = 0u;
	variableRateSettings.reprojectionFailedMode = vrt::EReprojectionFailedMode::_1x2;
	variableRateSettings.permutationSettings.maxVariableRate = vrt::EMaxVariableRate::_4x4;
	m_variableRateRenderer.Initialize(variableRateSettings);
}

void SpecularReflectionsRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		const RenderView& renderView = viewSpec.GetRenderView();

		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		const math::Vector2u resolution = viewSpec.GetRenderingHalfRes();

		m_variableRateRenderer.Reproject(graphBuilder, viewContext.motionHalfRes);

		const Bool hasValidHistory = viewContext.historyDepthHalfRes.IsValid() && viewContext.historyDepthHalfRes->GetResolution2D() == resolution;

		vrt::TracesAllocationDefinition tracesAllocationDefinition;
		tracesAllocationDefinition.debugName                        = RG_DEBUG_NAME("Specular Reflections");
		tracesAllocationDefinition.resolution                       = resolution;
		tracesAllocationDefinition.variableRateTexture              = graphBuilder.AcquireExternalTextureView(m_variableRateRenderer.GetVariableRateTexture());
		tracesAllocationDefinition.vrtPermutationSettings           = m_variableRateRenderer.GetPermutationSettings();
		tracesAllocationDefinition.outputTracesAndDispatchGroupsNum = true;
		tracesAllocationDefinition.traceIdx                         = renderView.GetRenderedFrameIdx();
		const vrt::TracesAllocation tracesAllocation = AllocateTraces(graphBuilder, tracesAllocationDefinition);

		SpecularReflectionsParams params;
		params.resolution              = resolution;
		params.normalsTexture          = viewContext.normalsHalfRes;
		params.historyNormalsTexture   = viewContext.historyNormalsHalfRes;
		params.roughnessTexture        = viewContext.roughnessHalfRes;
		params.historyRoughnessTexture = viewContext.historyRoughnessHalfRes;
		params.specularColorTexture    = viewContext.specularColorHalfRes;
		params.depthTexture            = viewContext.depthHalfRes;
		params.depthHistoryTexture     = viewContext.historyDepthHalfRes;
		params.linearDepthTexture      = viewContext.linearDepthHalfRes;
		params.motionTexture           = viewContext.motionHalfRes;
		params.skyViewLUT              = viewContext.skyViewLUT;

		const math::Vector2u reservoirsResolution = sr_restir::ComputeReservoirsResolution(resolution);
		const rg::RGBufferViewHandle reservoirsBuffer = sr_restir::CreateReservoirsBuffer(graphBuilder, reservoirsResolution);

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Initialize reservoirs"), reservoirsBuffer, 0u);

		trace::TraceParams traceParams;
		traceParams.tracesAllocation     = tracesAllocation;
		traceParams.reservoirsResolution = reservoirsResolution;
		traceParams.reservoirsBuffer     = reservoirsBuffer;

		trace::GenerateReservoirs(graphBuilder, renderScene, viewSpec, params, traceParams);

		const rg::RGTextureViewHandle luminanceHitDistanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Luminance Hit Distance Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));

		const rg::RGTextureViewHandle reservoirsBrightness = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reservoirs Brightness Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));

		sr_restir::ResamplingParams resamplingParams(renderView, renderScene);
		resamplingParams.initialReservoirBuffer         = reservoirsBuffer;
		resamplingParams.depthTexture                   = viewContext.depthHalfRes;
		resamplingParams.normalsTexture                 = viewContext.normalsHalfRes;
		resamplingParams.roughnessTexture               = viewContext.roughnessHalfRes;
		resamplingParams.specularColorTexture           = viewContext.specularColorHalfRes;
		resamplingParams.historyDepthTexture            = viewContext.historyDepthHalfRes;
		resamplingParams.historyNormalsTexture          = viewContext.historyNormalsHalfRes;
		resamplingParams.historyRoughnessTexture        = viewContext.historyRoughnessHalfRes;
		resamplingParams.historySpecularColorTexture    = viewContext.historySpecularColorHalfRes;
		resamplingParams.outLuminanceHitDistanceTexture = luminanceHitDistanceTexture;
		resamplingParams.motionTexture                  = viewContext.motionHalfRes;
		resamplingParams.tracesAllocation               = tracesAllocation;
		resamplingParams.enableTemporalResampling       = hasValidHistory;
		resamplingParams.spatialResamplingIterations    = 1u;

		const sr_restir::InitialResamplingResult initialResamplingResult = m_resampler.ExecuteInitialResampling(graphBuilder, resamplingParams);

		const vrt::TracesAllocation& additionalTracesAllocation = initialResamplingResult.additionalTracesAllocation;
		if (additionalTracesAllocation.IsValid())
		{
			trace::TraceParams additionalTracesParams;
			additionalTracesParams.tracesAllocation     = additionalTracesAllocation;
			additionalTracesParams.reservoirsResolution = reservoirsResolution;
			additionalTracesParams.reservoirsBuffer     = initialResamplingResult.resampledReservoirsBuffer;

			trace::GenerateReservoirs(graphBuilder, renderScene, viewSpec, params, additionalTracesParams);
		}

		m_resampler.ExecuteFinalResampling(graphBuilder, resamplingParams, initialResamplingResult);
		
		const rg::RGTextureViewHandle denoisedLuminanceTexture = denoise::Denoise(graphBuilder, renderScene, viewSpec, m_denoiser, params, luminanceHitDistanceTexture);

		upsampler::DepthBasedUpsampleParams upsampleParams;
		upsampleParams.debugName      = RG_DEBUG_NAME("Upsample Specular Reflections");
		upsampleParams.depth          = viewContext.depth;
		upsampleParams.depthHalfRes   = viewContext.depthHalfRes;
		upsampleParams.normalsHalfRes = viewContext.normalsHalfRes;
		upsampleParams.renderViewDS   = renderView.GetRenderViewDS();
		const rg::RGTextureViewHandle specularReflectionsFullRes = upsampler::DepthBasedUpsample(graphBuilder, denoisedLuminanceTexture, upsampleParams);

		const rg::RGTextureViewHandle specularReflectionsInfluenceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Reflections Influence Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_S_Float));

		RTReflectionsViewData reflectionsViewData;
		reflectionsViewData.reflectionsTexture           = specularReflectionsFullRes;
		reflectionsViewData.reflectionsInfluenceTexture  = specularReflectionsInfluenceTexture;
		reflectionsViewData.reservoirsBrightnessTexture  = reservoirsBrightness;

		viewSpec.GetData().Create<RTReflectionsViewData>(reflectionsViewData);

		viewSpec.GetRenderStageEntries(ERenderStage::CompositeLighting).GetPostRenderStage().AddRawMember(this, &SpecularReflectionsRenderStage::RenderVariableRateTexture);
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

void SpecularReflectionsRenderStage::RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const RTReflectionsViewData& reflectionsViewData = viewSpec.GetData().Get<RTReflectionsViewData>();

	const rdr::ShaderID vrtShader = vrt::CreateVariableRateTextureShader(m_variableRateRenderer.GetPermutationSettings());

	lib::MTHandle<vrt::RTVariableRateTextureDS> vrtDS = graphBuilder.CreateDescriptorSet<vrt::RTVariableRateTextureDS>(RENDERER_RESOURCE_NAME("RTVariableRateTextureDS"));
	vrtDS->u_influenceTexture = reflectionsViewData.reflectionsInfluenceTexture;
	vrtDS->u_roughnessTexture = viewContext.roughnessHalfRes;
	m_variableRateRenderer.Render(graphBuilder, reflectionsViewData.reservoirsBrightnessTexture, vrtShader, rg::BindDescriptorSets(vrtDS));
}

} // spt::rsc
