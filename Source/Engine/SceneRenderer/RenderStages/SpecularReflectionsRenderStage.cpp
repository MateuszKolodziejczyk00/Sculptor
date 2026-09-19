#include "SpecularReflectionsRenderStage.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereRenderSystem.h"
#include "SceneRenderSystems/DDGI/DDGIRenderSystem.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "Utils/SceneRenderingTypes.h"
#include "Utils/ScreenSpaceTracer.h"
#include "SceneRenderSystems/Lights/LightsRenderSystem.h"
#include "SceneRenderSystems/DDGI/DDGIRenderSystem.h"
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"
#include "ViewRenderSystems/ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "SceneRenderer/RenderStages/Utils/RayBinner.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "SceneRenderer/RenderStages/Utils/RTReflectionsTypes.h"
#include "SceneRenderer/Debug/Stats/RTReflectionsStatsView.h"
#include "MaterialsSubsystem.h"


SPT_DEFINE_LOG_CATEGORY(RTGI, true);


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::SpecularReflections, SpecularReflectionsRenderStage);

namespace renderer_params
{
RendererBoolParameter enableTemporalResampling("Enable Temporal Resampling", { "Specular Reflections" }, true);
RendererIntParameter spatialResamplingIterationsNum("Spatial Resampling Iterations Num", { "Specular Reflections" }, 1, 0, 2);

RendererBoolParameter halfResReflections("Half Res", { "Specular Reflections" }, false);
RendererBoolParameter forceFullRateTracingReflections("Force Full Rate Tracing", { "Specular Reflections" }, true);
RendererBoolParameter doFullFinalVisibilityCheck("Full Final Visibility Check", { "Specular Reflections" }, true);
RendererBoolParameter enableFireflyFilter("Enable Firefly Filter", { "Specular Reflections" }, true);
RendererBoolParameter enableSecondTracingPass("Enable SecondTracing Pass", { "Specular Reflections" }, false);
RendererBoolParameter blurVarianceEstimate("Blur Variance Estimate", { "Specular Reflections" }, false);
RendererBoolParameter useDepthTestForVRTReprojection("Use Depth Test For VRT Reprojection", { "Specular Reflections" }, false);
RendererBoolParameter enableSpatialResamplingSSVisibilityTest("Enable Spatial Resampling SS Visibility Test", { "Specular Reflections" }, false);
RendererBoolParameter useLargeTileSize("Use Large Tile Size", { "Specular Reflections" }, false);
RendererBoolParameter resampleOnlyFromTracedPixels("Resample Only From Traced Pixels", { "Specular Reflections" }, true);
RendererBoolParameter enableHitDistanceBasedMaxAge("Enable Hit Distance Based Max Age", { "Specular Reflections" }, true);
RendererIntParameter reservoirMaxAge("Reservoir Max Age", { "Specular Reflections" }, 10, 0, 30);
RendererFloatParameter resamplingRangeStep("Resampling Range Step", { "Specular Reflections" }, 2.333f, 0.f, 10.f);
RendererBoolParameter enableStableHistoryBlend("Enable Stable History Blend", { "Specular Reflections" }, false);
RendererIntParameter wideRadiusSpatialPassesNum("Wide Radius Spatial Passes Num", { "Specular Reflections" }, 0, 0, 5);
RendererBoolParameter useSharcAsRadianceCache("Use Sharc As Radiance Cache", { "Specular Reflections" }, true);
RendererIntParameter ssrtStepsNum("SSRT Steps Num", { "Specular Reflections" }, 10, 1, 64);
RendererFloatParameter ssrtTraceLenght("SSRT Trace Lenght", { "Specular Reflections" }, 0.15f, 0.f, 1.f);

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

	Bool resetAccumulation = false;
};


namespace trace
{

namespace ray_directions
{

BEGIN_SHADER_STRUCT(GenerateRayDirectionsConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                                resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                                invResolution)
	SHADER_STRUCT_FIELD(Uint32,                                        seed)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<Uint32>,                      tracesNum)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<vrt::EncodedRayTraceCommand>, traceCommands)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,             normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,             baseColorMetallicTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                     depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                     roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                    rwRaysDirections)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Real32>,                    rwRaysPdfs)
END_SHADER_STRUCT()


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
	shaderConstants.resolution               = resolution;
	shaderConstants.invResolution            = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.seed                     = lib::rnd::RandomFromTypeDomain<Uint32>();
	shaderConstants.tracesNum                = tracesAllocation.tracesNum;
	shaderConstants.traceCommands            = tracesAllocation.rayTraceCommands;
	shaderConstants.normalsTexture           = params.normalsTexture;
	shaderConstants.baseColorMetallicTexture = params.baseColorTexture;
	shaderConstants.depthTexture             = params.depthTexture;
	shaderConstants.roughnessTexture         = params.roughnessTexture;
	shaderConstants.rwRaysDirections         = rayDirectionsBuffer;
	shaderConstants.rwRaysPdfs               = rayPdfsBuffer;

	const rdr::PipelineStateID generateRayDirectionsPipeline = CreateGenerateRayDirectionsPipeline();

	graphBuilder.DispatchIndirect(RG_DEBUG_NAME("Generate Ray Directions"),
								  generateRayDirectionsPipeline,
								  tracesAllocation.dispatchIndirectArgs, 0,
								  rg::ShaderParams(shaderConstants, viewSpec.GetShadingViewContext().sharcCacheParams));

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


BEGIN_SHADER_STRUCT(RTShadingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                                      resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                                      invResolution)
	SHADER_STRUCT_FIELD(math::Vector2u,                                      reservoirsResolution)
	SHADER_STRUCT_FIELD(HeightFogParams,                                     heightFog)
	SHADER_STRUCT_FIELD(Uint32,                                              rayCommandsBufferSize)
	SHADER_STRUCT_FIELD(Uint32,                                              frameIdx)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,                   skyViewLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,                   transmittanceLUT)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,                       atmosphereParams)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint32>,                         rayDirections)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Real32>,                         rayPdfs)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                           depthTexture)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<RTHitMaterialInfo>,              hitMaterialInfos)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<sr_restir::SRPackedReservoir>, reservoirsBuffer)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<RaysShadingCounts>,              tracesNum)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<vrt::EncodedRayTraceCommand>,    traceCommands)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint32>,                         sortedTraces)
END_SHADER_STRUCT();


namespace miss_rays
{

static rdr::PipelineStateID CreateMissRaysShadingPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/MissRaysShading.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "MissRaysShadingCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("MissRaysShadingPipeline"), shader);
}


static void ShadeMissRays(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, const RTShadingParams& shadingParams, const rdr::GPUPtr<RTShadingConstants>& shadingConstants)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	static const rdr::PipelineStateID missRaysShadingPipeline = CreateMissRaysShadingPipeline();

	graphBuilder.DispatchIndirect(RG_DEBUG_NAME("Miss Rays Shading"),
								  missRaysShadingPipeline,
								  shadingParams.shadingIndirectArgs, ComputeMissShadingIndirectArgsOffset(),
								  rg::ShaderParams(shadingConstants, shadingViewContext.cloudscapeProbesParams));
}

} // miss_rays


namespace hit_rays
{

BEGIN_SHADER_STRUCT(HitRaysShadingPermutation)
	SHADER_STRUCT_FIELD(Bool,                    USE_DDGI)
	SHADER_STRUCT_FIELD(SharcShadersPermutation, SHARC_PERMUTATION)
END_SHADER_STRUCT()


RT_PSO(HitRayShadingPSO)
{
	RAY_GEN_SHADER("Sculptor/SpecularReflections/HitRaysShading.hlsl", HitRaysShadingRTG);

	MISS_SHADERS(SHADER_ENTRY("Sculptor/SpecularReflections/HitRaysShading.hlsl", GenericRTM));

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/SpecularReflections/HitRaysShading.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PERMUTATION_DOMAIN(HitRaysShadingPermutation);
	
	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const lib::DynamicArray<HitGroup> hitGroups = mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>();
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		CompilePermutation(compiler, psoDefinition, hitGroups, HitRaysShadingPermutation{ .USE_DDGI = true });
		CompilePermutation(compiler, psoDefinition, hitGroups, HitRaysShadingPermutation{ .USE_DDGI = false, .SHARC_PERMUTATION = SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = false } });
		CompilePermutation(compiler, psoDefinition, hitGroups, HitRaysShadingPermutation{ .USE_DDGI = false, .SHARC_PERMUTATION = SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = true } });
	}
};


static void ShadeHitRays(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, const RTShadingParams& shadingParams, const rdr::GPUPtr<RTShadingConstants>& shadingConstants)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	const LightsRenderSystem& lightsRenderSystem = rendererInterface.GetRenderSystemChecked<LightsRenderSystem>();

	const ddgi::DDGIRenderSystem& ddgiRenderSystem = rendererInterface.GetRenderSystemChecked<ddgi::DDGIRenderSystem>();

	const Bool useSharc = renderer_params::useSharcAsRadianceCache && SharcGICache::IsSharcSupported();

	HitRaysShadingPermutation permutation;
	permutation.USE_DDGI = !useSharc;
	if (useSharc)
	{
		permutation.SHARC_PERMUTATION = SharcGICache::GetShadersPermutation();
	}

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("Hit Rays Shading"),
								   HitRayShadingPSO::GetPermutation(permutation),
								   shadingParams.shadingIndirectArgs, ComputeHitShadingIndirectArgsOffset(),
								   rg::ShaderParams(shadingConstants,
													ddgiRenderSystem.GetDDGIGPUScene(),
													shadingViewContext.sharcCacheParams,
													lightsRenderSystem.GetGlobalLightsParams()));
}

} // hit_rays

static void ShadeRays(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& srParams, const RTShadingParams& shadingParams)
{
	SPT_PROFILER_FUNCTION();

	const AtmosphereRenderSystem& atmosphereSystem = rendererInterface.GetRenderSystemChecked<AtmosphereRenderSystem>();
	const AtmosphereContext& atmosphereContext     = atmosphereSystem.GetAtmosphereContext();

	const ParticipatingMediaViewRenderSystem& pmSystem = viewSpec.GetRenderSystemChecked<ParticipatingMediaViewRenderSystem>();

	RTShadingConstants rtShadingConstants;
	rtShadingConstants.resolution            = srParams.resolution;
	rtShadingConstants.invResolution         = srParams.resolution.cast<Real32>().cwiseInverse();
	rtShadingConstants.reservoirsResolution  = shadingParams.reservoirsResolution;
	rtShadingConstants.heightFog             = pmSystem.GetHeightFogParams();
	rtShadingConstants.rayCommandsBufferSize = static_cast<Uint32>(shadingParams.sortedTraces->GetSize() / sizeof(vrt::EncodedRayTraceCommand));
	rtShadingConstants.frameIdx              = viewSpec.GetFrameIdx();
	rtShadingConstants.skyViewLUT         = srParams.skyViewLUT;
	rtShadingConstants.transmittanceLUT   = atmosphereContext.transmittanceLUT;
	rtShadingConstants.atmosphereParams   = atmosphereContext.atmosphereParams;
	rtShadingConstants.hitMaterialInfos   = shadingParams.rtGBuffer.hitMaterialInfos;
	rtShadingConstants.rayDirections      = shadingParams.rtGBuffer.rayDirections;
	rtShadingConstants.rayPdfs            = shadingParams.rtGBuffer.rayPdfs;
	rtShadingConstants.depthTexture       = srParams.depthTexture;
	rtShadingConstants.reservoirsBuffer   = shadingParams.reservoirsBuffer;
	rtShadingConstants.traceCommands      = shadingParams.tracesAllocation.rayTraceCommands;
	rtShadingConstants.sortedTraces       = shadingParams.sortedTraces;
	rtShadingConstants.tracesNum          = shadingParams.raysShadingCounts;

	const rdr::GPUPtr<RTShadingConstants> shadingConstants = graphBuilder.CreateGPUData(rtShadingConstants);

	miss_rays::ShadeMissRays(graphBuilder, rendererInterface, renderScene, viewSpec, srParams, shadingParams, shadingConstants);
	hit_rays::ShadeHitRays(graphBuilder, rendererInterface, renderScene, viewSpec, srParams, shadingParams, shadingConstants);
}

} // shading


BEGIN_SHADER_STRUCT(SpecularReflectionsTraceConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                                       resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                                       invResolution)
	SHADER_STRUCT_FIELD(Uint32,                                               rayCommandsBufferSize)
	SHADER_STRUCT_FIELD(SSTracerData,                                         ssrTracer)
	SHADER_STRUCT_FIELD(Real32,                                               ssrTraceLength)
	SHADER_STRUCT_FIELD(GPUGBuffer,                                           gpuGBuffer)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<Uint32>,                             rayDirections)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<vrt::EncodedRayTraceCommand>,        traceCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<shading::RTHitMaterialInfo>,       hitMaterialInfos)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                           sortedRays) // hits first
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<shading::RaysShadingIndirectArgs>, shadingIndirectArgs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<shading::RaysShadingCounts>,       raysShadingCounts)
END_SHADER_STRUCT();


RT_PSO(SpecularReflectionsTracePSO)
{
	RAY_GEN_SHADER("Sculptor/SpecularReflections/RTGITrace.hlsl", GenerateRTGIRaysRTG);
	MISS_SHADERS(SHADER_ENTRY("Sculptor/SpecularReflections/RTGITrace.hlsl", GenericRTM));

	HIT_GROUP
	{
		CLOSEST_HIT_SHADER("Sculptor/SpecularReflections/RTGITrace.hlsl", GenericCHS);
		ANY_HIT_SHADER("Sculptor/SpecularReflections/RTGITrace.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		pso = CompilePSO(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>());
	}
};


struct TraceParams
{
	vrt::TracesAllocation tracesAllocation;

	math::Vector2u         reservoirsResolution;
	rg::RGBufferViewHandle reservoirsBuffer;
};


static void GenerateReservoirs(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, rg::RenderGraphDebugName debugName, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params, const TraceParams& traceParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, lib::String("Generate Reservoirs ") + debugName.AsString());

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	// Phase (1) Generate Ray Directions

	const auto [rayDirections, rayPdfs] = ray_directions::GenerateRayDirections(graphBuilder, renderScene, viewSpec, params, traceParams.tracesAllocation);

	// Phase (2) Trace Rays

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

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Shading Indirect Args"), shadingIndirectArgsBuffer, 0u);
	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Rays Shading Counts"), raysShadingCountsBuffer, 0u);


	SpecularReflectionsTraceConstants shaderConstants;
	shaderConstants.resolution            = params.resolution;
	shaderConstants.invResolution         = params.resolution.cast<Real32>().cwiseInverse();
	shaderConstants.rayCommandsBufferSize = static_cast<Uint32>(sortedRaysBuffer->GetSize() / sizeof(vrt::EncodedRayTraceCommand));
	shaderConstants.ssrTracer             = CreateScreenSpaceTracerData(viewContext.linearDepth, static_cast<Uint32>(renderer_params::ssrtStepsNum));
	shaderConstants.ssrTraceLength        = renderer_params::ssrtTraceLenght;
	shaderConstants.gpuGBuffer            = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);
	shaderConstants.rayDirections         = rayDirections;
	shaderConstants.traceCommands         = traceParams.tracesAllocation.rayTraceCommands;
	shaderConstants.hitMaterialInfos      = hitMaterialInfos;
	shaderConstants.sortedRays            = sortedRaysBuffer;
	shaderConstants.shadingIndirectArgs   = shadingIndirectArgsBuffer;
	shaderConstants.raysShadingCounts     = raysShadingCountsBuffer;

	graphBuilder.TraceRaysIndirect(RG_DEBUG_NAME("Specular Reflections Trace Rays"),
								   SpecularReflectionsTracePSO::pso,
								   traceParams.tracesAllocation.tracingIndirectArgs, 0u,
								   rg::ShaderParams(shaderConstants));

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

	shading::ShadeRays(graphBuilder, rendererInterface, renderScene, viewSpec, params, shadingParams);
}

} // trace

namespace denoise
{

static sr_denoiser::Denoiser::Result Denoise(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, sr_denoiser::Denoiser& denoiser, const SpecularReflectionsParams& params, rg::RGTextureViewHandle specularReflections, rg::RGTextureViewHandle diffuseReflections, rg::RGTextureViewHandle lightDirection)
{
	SPT_PROFILER_FUNCTION();

	sr_denoiser::Denoiser::Params denoiserParams(viewSpec);
	denoiserParams.currentDepthTexture      = params.depthTexture;
	denoiserParams.historyDepthTexture      = params.depthHistoryTexture;
	denoiserParams.linearDepthTexture       = params.linearDepthTexture;
	denoiserParams.motionTexture            = params.motionTexture;
	denoiserParams.normalsTexture           = params.normalsTexture;
	denoiserParams.historyNormalsTexture    = params.historyNormalsTexture;
	denoiserParams.roughnessTexture         = params.roughnessTexture;
	denoiserParams.historyRoughnessTexture  = params.historyRoughnessTexture;
	denoiserParams.specularTexture          = specularReflections;
	denoiserParams.diffuseTexture           = diffuseReflections;
	denoiserParams.lightDirection           = lightDirection;
	denoiserParams.blurVarianceEstimate     = !renderer_params::forceFullRateTracingReflections && renderer_params::blurVarianceEstimate;
	denoiserParams.enableStableHistoryBlend = renderer_params::enableStableHistoryBlend;
	denoiserParams.wideRadiusPassesNum      = renderer_params::wideRadiusSpatialPassesNum;
	denoiserParams.resetAccumulation        = params.resetAccumulation;
	denoiserParams.baseColorMetallicTexture = params.baseColorTexture;

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
	SHADER_STRUCT_FIELD(Uint32,                            forceFullRateTracing)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, influenceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, varianceEstimation)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         linearDepthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, specularReflectionsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, diffuseReflectionsTexture)
END_SHADER_STRUCT();

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

void SpecularReflectionsRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	if (rdr::GPUApi::IsRayTracingEnabled())
	{
		SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RT Reflections");

		UpdateSharcCache(graphBuilder, rendererInterface, renderScene, viewSpec, stageContext);

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
		tracesAllocationDefinition.traceIdx                         = viewSpec.GetFrameIdx();
		const vrt::TracesAllocation tracesAllocation = AllocateTraces(graphBuilder, tracesAllocationDefinition);

		SpecularReflectionsParams params;
		params.resolution        = resolution;
		params.skyViewLUT        = viewContext.skyViewLUT;
		params.resetAccumulation = stageContext.rendererSettings.resetAccumulation;

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

		trace::GenerateReservoirs(graphBuilder, rendererInterface, RG_DEBUG_NAME("1st Pass"), renderScene, viewSpec, params, traceParams);

		const rg::RGTextureViewHandle specularLuminanceHitDistanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Luminance Hit Distance Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));
		const rg::RGTextureViewHandle diffuseLuminanceHitDistanceTexture  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Diffuse Hit Distance Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));
		const rg::RGTextureViewHandle lightDirectionTexture               = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Light Direction Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RG16_UN_Float));

		const lib::StaticArray<sr_restir::SpatialResamplingPassParams, 2u> spatialResamplingPasses =
		{
			sr_restir::SpatialResamplingPassParams{
				.resamplingRangeMultiplier        = 1.f,
				.samplesNum                       = 4u,
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

		const Bool enableDistanceBasedMaxAge = renderer_params::enableHitDistanceBasedMaxAge && !viewSystemsInfo.useUnifiedDenoising;

		sr_restir::ResamplingParams resamplingParams(viewSpec, renderScene);
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
		resamplingParams.outLightDirectionTexture        = lightDirectionTexture;
		resamplingParams.motionTexture                   = params.motionTexture;
		resamplingParams.historySpecularHitDist          = !viewSystemsInfo.useUnifiedDenoising ? m_denoiser.GetHistorySpecularHitDist(graphBuilder) : nullptr;
		resamplingParams.tracesAllocation                = tracesAllocation;
		resamplingParams.vrReprojectionSuccessMask       = vrReprojectionSuccessMask;
		resamplingParams.enableTemporalResampling        = hasValidHistory && renderer_params::enableTemporalResampling;
		resamplingParams.spatialResamplingPasses         = activeResamplingPasses;
		resamplingParams.doFullFinalVisibilityCheck      = renderer_params::doFullFinalVisibilityCheck;
		resamplingParams.enableSecondTracingPass         = renderer_params::enableSecondTracingPass;
		resamplingParams.variableRateTileSizeBitOffset   = vrt::GetTileSizeBitOffset(m_variableRateRenderer.GetVariableRateSettings());
		resamplingParams.enableHitDistanceBasedMaxAge    = enableDistanceBasedMaxAge;
		resamplingParams.enableFireflyFilter             = renderer_params::enableFireflyFilter && !viewSystemsInfo.useUnifiedDenoising;
		resamplingParams.reservoirMaxAge                 = renderer_params::reservoirMaxAge;
		resamplingParams.resamplingRangeStep             = renderer_params::resamplingRangeStep;
		resamplingParams.ssrStepsNum                     = renderer_params::ssrtStepsNum;
		resamplingParams.ssrTraceLength                  = renderer_params::ssrtTraceLenght;

		const sr_restir::InitialResamplingResult initialResamplingResult = m_resampler.ExecuteInitialResampling(graphBuilder, resamplingParams);

		const vrt::TracesAllocation& additionalTracesAllocation = initialResamplingResult.additionalTracesAllocation;
		if (additionalTracesAllocation.IsValid())
		{
			trace::TraceParams additionalTracesParams;
			additionalTracesParams.tracesAllocation     = additionalTracesAllocation;
			additionalTracesParams.reservoirsResolution = reservoirsResolution;
			additionalTracesParams.reservoirsBuffer     = initialResamplingResult.resampledReservoirsBuffer;

			trace::GenerateReservoirs(graphBuilder, rendererInterface, RG_DEBUG_NAME("2nd Pass"), renderScene, viewSpec, params, additionalTracesParams);
		}

		m_resampler.ExecuteFinalResampling(graphBuilder, resamplingParams, initialResamplingResult);

		rg::RGTextureViewHandle specularReflectionsFullRes;
		rg::RGTextureViewHandle diffuseReflectionsFullRes;
		rg::RGTextureViewHandle varianceEstimation;

		if (viewSystemsInfo.useUnifiedDenoising)
		{
			m_denoiser.InvalidateHistory();

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
																				  diffuseLuminanceHitDistanceTexture,
																				  lightDirectionTexture);

			if (isHalfRes)
			{
				{
					upsampler::DepthBasedUpsampleParams specularUpsampleParams;
					specularUpsampleParams.debugName               = RG_DEBUG_NAME("Upsample Specular Reflections");
					specularUpsampleParams.depth                   = viewContext.depth;
					specularUpsampleParams.depthHalfRes            = viewContext.depthHalfRes;
					specularUpsampleParams.normalsHalfRes          = viewContext.normalsHalfRes;
					specularUpsampleParams.fireflyFilteringEnabled = true;
					specularReflectionsFullRes = upsampler::DepthBasedUpsample(graphBuilder, denoiserResult.denoisedSpecular, specularUpsampleParams);
				}

				{
					upsampler::DepthBasedUpsampleParams diffuseUpsampleParams;
					diffuseUpsampleParams.debugName               = RG_DEBUG_NAME("Upsample Diffuse Reflections");
					diffuseUpsampleParams.depth                   = viewContext.depth;
					diffuseUpsampleParams.depthHalfRes            = viewContext.depthHalfRes;
					diffuseUpsampleParams.normalsHalfRes          = viewContext.normalsHalfRes;
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
		reflectionsViewData.finalDiffuseGI       = diffuseReflectionsFullRes;
		reflectionsViewData.finalSpecularGI      = specularReflectionsFullRes;

		reflectionsViewData.reflectionsInfluenceTexture  = reflectionsInfluenceTexture;
		reflectionsViewData.varianceEstimation           = varianceEstimation;
		reflectionsViewData.halfResReflections           = isHalfRes;

		viewSpec.GetBlackboard().Create<RTReflectionsViewData>(reflectionsViewData);

		viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderVariableRateTexture).AddRawMember(this, &SpecularReflectionsRenderStage::RenderVariableRateTexture);

#if SPT_ENABLE_SCENE_RENDERER_STATS
		stats::RecordStatsSample(graphBuilder, resolution, tracesAllocation, additionalTracesAllocation);
#endif // SPT_ENABLE_SCENE_RENDERER_STATS
	}
}

void SpecularReflectionsRenderStage::RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const RTReflectionsViewData& reflectionsViewData = viewSpec.GetBlackboard().Get<RTReflectionsViewData>();

	const Bool isHalfRes = reflectionsViewData.halfResReflections;

	const rdr::ShaderID vrtShader = vrt::CreateVariableRateTextureShader(m_variableRateRenderer.GetPermutationSettings());

	{
		vrt::VariableRateRTConstants shaderConstants;
		shaderConstants.forceFullRateTracing = renderer_params::forceFullRateTracingReflections;
		shaderConstants.influenceTexture           = reflectionsViewData.reflectionsInfluenceTexture;
		shaderConstants.roughnessTexture           = isHalfRes ? viewContext.roughnessHalfRes : viewContext.gBuffer[GBuffer::Texture::Roughness];
		shaderConstants.varianceEstimation         = reflectionsViewData.varianceEstimation;
		shaderConstants.linearDepthTexture         = isHalfRes ? viewContext.linearDepthHalfRes : viewContext.linearDepth;
		shaderConstants.specularReflectionsTexture = reflectionsViewData.finalSpecularGI;
		shaderConstants.diffuseReflectionsTexture  = reflectionsViewData.finalDiffuseGI;

		rg::BindShaderParamsScope bindShaderParamsScope(graphBuilder, rg::ShaderParams(shaderConstants));

		m_variableRateRenderer.Render(graphBuilder, nullptr, vrtShader);
	}
}

void SpecularReflectionsRenderStage::UpdateSharcCache(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	if (renderer_params::useSharcAsRadianceCache && !SharcGICache::IsSharcSupported())
	{
		SPT_LOG_WARN(RTGI, "Sharc is not supported. Falling back to DDGI.");
		renderer_params::useSharcAsRadianceCache.SetValue(false);
	}

	if (renderer_params::useSharcAsRadianceCache && SharcGICache::IsSharcSupported())
	{
		if (!m_sharcCache)
		{
			m_sharcCache = std::make_unique<SharcGICache>();
		}

		SharcUpdateParams updateParams;
		updateParams.resetCache     = stageContext.rendererSettings.resetAccumulation;
		updateParams.ssrStepsNum    = renderer_params::ssrtStepsNum;
		updateParams.ssrTraceLength = renderer_params::ssrtTraceLenght;
		m_sharcCache->Update(graphBuilder, rendererInterface, renderScene, viewSpec, updateParams);
	}
	else
	{
		m_sharcCache.reset();
	}
}

} // spt::rsc
