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

BEGIN_SHADER_STRUCT(SpecularTraceShaderParams)
	SHADER_STRUCT_FIELD(math::Vector2f, random)
	SHADER_STRUCT_FIELD(Real32,         volumetricFogNear)
	SHADER_STRUCT_FIELD(Real32,         volumetricFogFar)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT()


DS_BEGIN(SpecularReflectionsTraceDS, rg::RGDescriptorSetState<SpecularReflectionsTraceDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<SceneRayTracingDS>),                             rayTracingDS)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_shadingNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SpecularTraceShaderParams>),              u_traceParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),                           u_integratedInScatteringTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<sr_restir::SRPackedReservoir>),       u_reservoirsBuffer)
DS_END();


static rdr::PipelineStateID CreateSpecularReflectionsTracePipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	rdr::RayTracingPipelineShaders rtShaders;
	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/SpecularReflectionsTrace.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "GenerateSpecularReflectionsRaysRTG"));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/SpecularReflectionsTrace.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "SpecularReflectionsRTM")));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/SpecularReflectionsTrace.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "SpecularReflectionsShadowsRTM")));

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

	RayTracingRenderSceneSubsystem& rayTracingSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	static rdr::PipelineStateID traceRaysPipeline;
	if (!traceRaysPipeline.IsValid() || rayTracingSubsystem.AreSBTRecordsDirty())
	{
		traceRaysPipeline = CreateSpecularReflectionsTracePipeline(rayTracingSubsystem);
	}

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& atmosphereContext = atmosphereSubsystem.GetAtmosphereContext();
	
	ParticipatingMediaViewRenderSystem& participatingMediaViewSystem = renderView.GetRenderSystem<ParticipatingMediaViewRenderSystem>();
	const VolumetricFogParams& fogParams = participatingMediaViewSystem.GetVolumetricFogParams();

	const rg::RGBufferViewHandle reservoirsBuffer = sr_restir::CreateReservoirsBuffer(graphBuilder, params.resolution);

	SpecularTraceShaderParams shaderParams;
	shaderParams.random            = math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	shaderParams.volumetricFogNear = fogParams.nearPlane;
	shaderParams.volumetricFogFar  = fogParams.farPlane;
	shaderParams.resolution        = params.resolution;

	lib::MTHandle<SpecularReflectionsTraceDS> traceRaysDS = graphBuilder.CreateDescriptorSet<SpecularReflectionsTraceDS>(RENDERER_RESOURCE_NAME("SpecularReflectionsTraceDS"));
	traceRaysDS->u_skyViewLUT                    = params.skyViewLUT;
	traceRaysDS->u_transmittanceLUT              = atmosphereContext.transmittanceLUT;
	traceRaysDS->u_atmosphereParams              = atmosphereContext.atmosphereParamsBuffer->CreateFullView();
	traceRaysDS->rayTracingDS                    = rayTracingSubsystem.GetSceneRayTracingDS();
	traceRaysDS->u_shadingNormalsTexture         = params.normalsTexture;
	traceRaysDS->u_depthTexture                  = params.depthTexture;
	traceRaysDS->u_roughnessTexture              = params.roughnessTexture;
	traceRaysDS->u_traceParams                   = shaderParams;
	traceRaysDS->u_integratedInScatteringTexture = fogParams.integratedInScatteringTextureView;
	traceRaysDS->u_reservoirsBuffer              = reservoirsBuffer;

	const LightsRenderSystem& lightsRenderSystem        = renderScene.GetRenderSystemChecked<LightsRenderSystem>();
	const ShadowMapsManagerSubsystem& shadowMapsManager = renderScene.GetSceneSubsystemChecked<ShadowMapsManagerSubsystem>();

	const ddgi::DDGISceneSubsystem& ddgiSceneSubsystem = renderScene.GetSceneSubsystemChecked<ddgi::DDGISceneSubsystem>();
	lib::MTHandle<ddgi::DDGISceneDS> ddgiDS = ddgiSceneSubsystem.GetDDGISceneDS();

	graphBuilder.TraceRays(RG_DEBUG_NAME("Specular Reflections Trace Rays"),
						   traceRaysPipeline,
						   params.resolution,
						   rg::BindDescriptorSets(std::move(traceRaysDS),
												  renderView.GetRenderViewDS(),
												  std::move(ddgiDS),
												  lightsRenderSystem.GetGlobalLightsDS(),
												  shadowMapsManager.GetShadowMapsDS(),
												  mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
												  GeometryManager::Get().GetGeometryDSState(),
												  StaticMeshUnifiedData::Get().GetUnifiedDataDS()));

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
};


DS_BEGIN(ApplySpecularReflectionsDS, rg::RGDescriptorSetState<ApplySpecularReflectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),  u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>), u_specularReflectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_baseColorMetallicTexture)
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
		applyParams.luminanceTexture = viewContext.luminance;
		applyParams.specularReflectionsTexture = specularReflectionsFullRes;
		applyParams.baseColorMetallicTexture = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
		apply::ApplySpecularReflections(graphBuilder, viewSpec, applyParams);
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
