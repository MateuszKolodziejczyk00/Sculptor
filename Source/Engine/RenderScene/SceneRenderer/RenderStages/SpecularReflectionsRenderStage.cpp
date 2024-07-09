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


namespace spt::rsc
{
REGISTER_RENDER_STAGE(ERenderStage::SpecularReflections, SpecularReflectionsRenderStage);

struct SpecularReflectionsParams
{
	math::Vector2u resolution;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle depthHistoryTexture;
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
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                   u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                           u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                           u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                    u_rwRaysDirectionTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                            u_rwRaysPdfTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GenerateRayDirectionsConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateGenerateRayDirectionsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/GenerateRayDirections.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateRayDirectionsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateRayDirectionsPipeline"), shader);
}


struct RayDirections
{
	rg::RGTextureViewHandle rayDirectionsTexture;
	rg::RGTextureViewHandle rayPdfsTexture;
};


RayDirections GenerateRayDirections(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u resolution = params.resolution;

	const rg::RGTextureViewHandle rayDirectionsTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Ray Directions Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::RG16_UN_Float));
	const rg::RGTextureViewHandle rayPdfsTexture       = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Ray PDFs Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::R32_S_Float));

	GenerateRayDirectionsConstants shaderConstants;
	shaderConstants.random        = math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx      = renderView.GetRenderedFrameIdx();

	lib::MTHandle<GenerateRayDirectionsDS> generateRayDirectionsDS = graphBuilder.CreateDescriptorSet<GenerateRayDirectionsDS>(RENDERER_RESOURCE_NAME("GenerateRayDirectionsDS"));
	generateRayDirectionsDS->u_normalsTexture          = params.normalsTexture;
	generateRayDirectionsDS->u_depthTexture            = params.depthTexture;
	generateRayDirectionsDS->u_roughnessTexture        = params.roughnessTexture;
	generateRayDirectionsDS->u_rwRaysDirectionTexture  = rayDirectionsTexture;
	generateRayDirectionsDS->u_rwRaysPdfTexture        = rayPdfsTexture;
	generateRayDirectionsDS->u_constants               = shaderConstants;

	const rdr::PipelineStateID generateRayDirectionsPipeline = CreateGenerateRayDirectionsPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate Ray Directions"),
						  generateRayDirectionsPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(generateRayDirectionsDS), renderView.GetRenderViewDS()));

	return RayDirections{ rayDirectionsTexture, rayPdfsTexture };
}

} // ray_directions

namespace shading
{

struct RTGBuffer
{
	rg::RGTextureViewHandle hitNormal;
	rg::RGTextureViewHandle hitBaseColorMetallic;
	rg::RGTextureViewHandle hitRoughness;
	rg::RGTextureViewHandle rayDistance;

	rg::RGTextureViewHandle rayDirection;
	rg::RGTextureViewHandle rayPdf;
};


struct RTShadingParams
{
	RTGBuffer rtGBuffer;

	math::Vector2u reservoirsResolution;
	rg::RGBufferViewHandle reservoirsBuffer;
};


BEGIN_SHADER_STRUCT(RTShadingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector2u, reservoirsResolution)
END_SHADER_STRUCT()


DS_BEGIN(RTShadingDS, rg::RGDescriptorSetState<RTShadingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_hitNormalTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_hitBaseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_hitRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_rayDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_rayDirectionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_rayPdfTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<sr_restir::SRPackedReservoir>),       u_reservoirsBuffer)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RTShadingConstants>),                     u_constants)
DS_END();

namespace miss_rays
{

static rdr::PipelineStateID CreateMissRaysShadingPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/MissRaysShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "MissRaysShadingCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("MissRaysShadingPipeline"), shader);
}


static void ShadeMissRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, lib::MTHandle<RTShadingDS> shadingDS)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	static const rdr::PipelineStateID missRaysShadingPipeline = CreateMissRaysShadingPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Miss Rays Shading"),
						  missRaysShadingPipeline,
						  math::Utils::DivideCeil(srParams.resolution, math::Vector2u(8u, 4u)),
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



static void ShadeHitRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, lib::MTHandle<RTShadingDS> shadingDS)
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

	graphBuilder.TraceRays(RG_DEBUG_NAME("Hit Rays Shading"),
						  hitRaysShadingPipeline,
						  srParams.resolution,
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
	shadingDS->u_hitNormalTexture            = shadingParams.rtGBuffer.hitNormal;
	shadingDS->u_hitBaseColorMetallicTexture = shadingParams.rtGBuffer.hitBaseColorMetallic;
	shadingDS->u_hitRoughnessTexture         = shadingParams.rtGBuffer.hitRoughness;
	shadingDS->u_rayDistanceTexture          = shadingParams.rtGBuffer.rayDistance;
	shadingDS->u_rayDirectionTexture         = shadingParams.rtGBuffer.rayDirection;
	shadingDS->u_rayPdfTexture               = shadingParams.rtGBuffer.rayPdf;
	shadingDS->u_depthTexture                = srParams.depthTexture;
	shadingDS->u_reservoirsBuffer            = shadingParams.reservoirsBuffer;
	shadingDS->u_constants                   = rtShadingConstants;

	miss_rays::ShadeMissRays(graphBuilder, renderScene, viewSpec, srParams, shadingDS);
	hit_rays::ShadeHitRays(graphBuilder, renderScene, viewSpec, srParams, shadingDS);
}

} // shading


DS_BEGIN(SpecularReflectionsTraceDS, rg::RGDescriptorSetState<SpecularReflectionsTraceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<SceneRayTracingDS>),                             rayTracingDS)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_rayDirectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                   u_reorderingsTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                            u_hitNormalTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_hitBaseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                    u_hitRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                    u_rayDistanceTexture)
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


static rg::RGBufferViewHandle GenerateInitialReservoirs(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	// Phase (1) Generate Ray Directions + Ray Binning

	const auto [rayDirectionsTexture, rayPdfsTexture] = ray_directions::GenerateRayDirections(graphBuilder, renderScene, viewSpec, params);
	const rg::RGTextureViewHandle reorderingsTexture  = ray_binner::ExecuteRayBinning(graphBuilder, rayDirectionsTexture);

	// Phase (2) Trace Rays

	RayTracingRenderSceneSubsystem& rayTracingSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	static rdr::PipelineStateID traceRaysPipeline;
	if (!traceRaysPipeline.IsValid() || rayTracingSubsystem.AreSBTRecordsDirty())
	{
		traceRaysPipeline = CreateSpecularReflectionsTracePipeline(rayTracingSubsystem);
	}

	const rg::RGTextureViewHandle hitNormal            = graphBuilder.CreateTextureView(RG_DEBUG_NAME("RT Hit Normal"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RG16_UN_Float));
	const rg::RGTextureViewHandle hitBaseColorMetallic = graphBuilder.CreateTextureView(RG_DEBUG_NAME("RT Hit Base Color Metallic"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA8_UN_Float));
	const rg::RGTextureViewHandle hitRoughness         = graphBuilder.CreateTextureView(RG_DEBUG_NAME("RT Roughness"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::R16_UN_Float));
	const rg::RGTextureViewHandle rayDistance          = graphBuilder.CreateTextureView(RG_DEBUG_NAME("RT Ray Distance"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::R32_S_Float));

	lib::MTHandle<SpecularReflectionsTraceDS> traceRaysDS = graphBuilder.CreateDescriptorSet<SpecularReflectionsTraceDS>(RENDERER_RESOURCE_NAME("SpecularReflectionsTraceDS"));
	traceRaysDS->rayTracingDS                  = rayTracingSubsystem.GetSceneRayTracingDS();
	traceRaysDS->u_depthTexture                = params.depthTexture;
	traceRaysDS->u_rayDirectionsTexture        = rayDirectionsTexture;
	traceRaysDS->u_reorderingsTexture          = reorderingsTexture;
	traceRaysDS->u_hitNormalTexture            = hitNormal;
	traceRaysDS->u_hitBaseColorMetallicTexture = hitBaseColorMetallic;
	traceRaysDS->u_hitRoughnessTexture         = hitRoughness;
	traceRaysDS->u_rayDistanceTexture          = rayDistance;

	graphBuilder.TraceRays(RG_DEBUG_NAME("Specular Reflections Trace Rays"),
						   traceRaysPipeline,
						   params.resolution,
						   rg::BindDescriptorSets(std::move(traceRaysDS),
												  renderView.GetRenderViewDS(),
												  mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
												  GeometryManager::Get().GetGeometryDSState(),
												  StaticMeshUnifiedData::Get().GetUnifiedDataDS()));

	// Phase (3) Shade Rays

	const math::Vector2u reservoirsResolution = sr_restir::ComputeReservoirsResolution(params.resolution);
	const rg::RGBufferViewHandle reservoirsBuffer = sr_restir::CreateReservoirsBuffer(graphBuilder, reservoirsResolution);

	shading::RTGBuffer rtGBuffer;
	rtGBuffer.hitNormal            = hitNormal;
	rtGBuffer.hitBaseColorMetallic = hitBaseColorMetallic;
	rtGBuffer.hitRoughness         = hitRoughness;
	rtGBuffer.rayDistance          = rayDistance;
	rtGBuffer.rayDirection         = rayDirectionsTexture;
	rtGBuffer.rayPdf               = rayPdfsTexture;

	shading::RTShadingParams shadingParams;
	shadingParams.rtGBuffer            = rtGBuffer;
	shadingParams.reservoirsResolution = reservoirsResolution;
	shadingParams.reservoirsBuffer     = reservoirsBuffer;

	shading::ShadeRays(graphBuilder, renderScene, viewSpec, params, shadingParams);

	return reservoirsBuffer;
}

} // trace

namespace denoise
{

static rg::RGTextureViewHandle Denoise(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle denoisedTexture, sr_denoiser::Denoiser& denoiser, const SpecularReflectionsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	const RenderSceneEntityHandle viewEntityHandle = renderView.GetViewEntity();

	sr_denoiser::Denoiser::Params denoiserParams(renderView);
	denoiserParams.currentDepthTexture     = params.depthTexture;
	denoiserParams.historyDepthTexture     = params.depthHistoryTexture;
	denoiserParams.motionTexture           = params.motionTexture;
	denoiserParams.normalsTexture          = params.normalsTexture;
	denoiserParams.historyNormalsTexture   = params.historyNormalsTexture;
	denoiserParams.roughnessTexture        = params.roughnessTexture;
	denoiserParams.historyRoughnessTexture = params.historyRoughnessTexture;

	return denoiser.Denoise(graphBuilder, denoisedTexture, denoiserParams);
}

} // denoise

namespace apply
{

struct ApplySpecularReflectionsParams
{
	rg::RGTextureViewHandle luminanceTexture;
	rg::RGTextureViewHandle specularReflectionsTexture;
	rg::RGTextureViewHandle baseColorMetallicTexture;
	rg::RGTextureViewHandle roughnessTexture;
	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle tangentFrameTexture;
};


DS_BEGIN(ApplySpecularReflectionsDS, rg::RGDescriptorSetState<ApplySpecularReflectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_specularReflectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_baseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_brdfIntegrationLUTSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_tangentFrameTexture)
DS_END();


static rdr::PipelineStateID CompileApplySpecularReflectionsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/ApplySpecularReflections.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ApplySpecularReflectionsCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ApplySpecularReflectionsPipeline"), shader);
}


static void ApplySpecularReflections(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ApplySpecularReflectionsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u resolution = params.luminanceTexture->GetResolution2D();

	static rdr::PipelineStateID applySpecularReflectionsPipeline = CompileApplySpecularReflectionsPipeline();

	lib::MTHandle<ApplySpecularReflectionsDS> applySpecularReflectionsDS = graphBuilder.CreateDescriptorSet<ApplySpecularReflectionsDS>(RENDERER_RESOURCE_NAME("ApplySpecularReflectionsDS"));
	applySpecularReflectionsDS->u_luminanceTexture           = params.luminanceTexture;
	applySpecularReflectionsDS->u_specularReflectionsTexture = params.specularReflectionsTexture;
	applySpecularReflectionsDS->u_baseColorMetallicTexture   = params.baseColorMetallicTexture;
	applySpecularReflectionsDS->u_brdfIntegrationLUT         = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
	applySpecularReflectionsDS->u_roughnessTexture           = params.roughnessTexture;
	applySpecularReflectionsDS->u_depthTexture               = params.depthTexture;
	applySpecularReflectionsDS->u_tangentFrameTexture        = params.tangentFrameTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Specular Reflections"),
						  applySpecularReflectionsPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(applySpecularReflectionsDS), renderView.GetRenderViewDS()));
}

} // apply


SpecularReflectionsRenderStage::SpecularReflectionsRenderStage()
	: m_denoiser(RG_DEBUG_NAME("SR Denoiser"))
{ }

void SpecularReflectionsRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		const RenderView& renderView = viewSpec.GetRenderView();

		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		const math::Vector2u halfResolution = viewSpec.GetRenderingHalfRes();

		const Bool hasValidHistory = viewContext.historyDepthHalfRes.IsValid() && viewContext.historyDepthHalfRes->GetResolution2D() == halfResolution;

		SpecularReflectionsParams params;
		params.resolution              = halfResolution;
		params.normalsTexture          = viewContext.normalsHalfRes;
		params.historyNormalsTexture   = viewContext.historyNormalsHalfRes;
		params.roughnessTexture        = viewContext.roughnessHalfRes;
		params.historyRoughnessTexture = viewContext.historyRoughnessHalfRes;
		params.specularColorTexture    = viewContext.specularColorHalfRes;
		params.depthTexture            = viewContext.depthHalfRes;
		params.depthHistoryTexture     = viewContext.historyDepthHalfRes;
		params.motionTexture           = viewContext.motionHalfRes;
		params.skyViewLUT              = viewContext.skyViewLUT;

		const rg::RGBufferViewHandle initialReservoirs = trace::GenerateInitialReservoirs(graphBuilder, renderScene, viewSpec, params);

		const rg::RGTextureViewHandle luminanceHitDistanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Luminance Hit Distance Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));

		sr_restir::ResamplingParams resamplingParams(renderView, renderScene);
		resamplingParams.initialReservoirBuffer         = initialReservoirs;
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
		resamplingParams.enableTemporalResampling       = hasValidHistory;

		m_resampler.Resample(graphBuilder, resamplingParams);
		
		const rg::RGTextureViewHandle denoisedLuminanceTexture = denoise::Denoise(graphBuilder, renderScene, viewSpec, luminanceHitDistanceTexture, m_denoiser, params);

		upsampler::DepthBasedUpsampleParams upsampleParams;
		upsampleParams.debugName          = RG_DEBUG_NAME("Upsample Specular Reflections");
		upsampleParams.depth              = viewContext.depth;
		upsampleParams.depthHalfRes       = viewContext.depthHalfRes;
		upsampleParams.normalsHalfRes     = viewContext.normalsHalfRes;
		upsampleParams.renderViewDS       = renderView.GetRenderViewDS();
		const rg::RGTextureViewHandle specularReflectionsFullRes = upsampler::DepthBasedUpsample(graphBuilder, denoisedLuminanceTexture, upsampleParams);

		apply::ApplySpecularReflectionsParams applyParams;
		applyParams.luminanceTexture           = viewContext.luminance;
		applyParams.specularReflectionsTexture = specularReflectionsFullRes;
		applyParams.baseColorMetallicTexture   = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
		applyParams.roughnessTexture           = viewContext.gBuffer[GBuffer::Texture::Roughness];
		applyParams.depthTexture               = viewContext.depth;
		applyParams.tangentFrameTexture        = viewContext.gBuffer[GBuffer::Texture::TangentFrame];
		apply::ApplySpecularReflections(graphBuilder, viewSpec, applyParams);
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
