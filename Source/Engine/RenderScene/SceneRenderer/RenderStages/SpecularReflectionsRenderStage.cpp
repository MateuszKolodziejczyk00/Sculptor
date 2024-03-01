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
#include "Denoisers/SpecularReflectionsDenoiser/SRDenoiser.h"
#include "Utils/ShadingTexturesDownsampler.h"
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
	rg::RGTextureViewHandle specularColorRoughnessTexture;
	rg::RGTextureViewHandle skyViewLUT;
};


struct SpecularReflectionsViewData
{
	SpecularReflectionsViewData()
		: denoiser(RG_DEBUG_NAME("SR Denoiser"))
	{ }

	sr_denoiser::Denoiser denoiser;
};

namespace trace
{

BEGIN_SHADER_STRUCT(SpecularTraceShaderParams)
	SHADER_STRUCT_FIELD(math::Vector2f, random)
	SHADER_STRUCT_FIELD(Real32, volumetricFogNear)
	SHADER_STRUCT_FIELD(Real32, volumetricFogFar)
END_SHADER_STRUCT()


DS_BEGIN(SpecularReflectionsTraceDS, rg::RGDescriptorSetState<SpecularReflectionsTraceDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_reflectedLuminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<SceneRayTracingDS>),                             rayTracingDS)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_shadingNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_specularAndRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SpecularTraceShaderParams>),              u_traceParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),                           u_integratedInScatteringTexture)
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


static rg::RGTextureViewHandle TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SpecularReflectionsParams& params)
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
	
	const rg::RGTextureViewHandle reflectedLuminanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Reflections Texture"), rg::TextureDef(params.resolution, rhi::EFragmentFormat::RGBA16_S_Float));

	ParticipatingMediaViewRenderSystem& participatingMediaViewSystem = renderView.GetRenderSystem<ParticipatingMediaViewRenderSystem>();
	const VolumetricFogParams& fogParams = participatingMediaViewSystem.GetVolumetricFogParams();

	SpecularTraceShaderParams shaderParams;
	shaderParams.random            = math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	shaderParams.volumetricFogNear = fogParams.nearPlane;
	shaderParams.volumetricFogFar  = fogParams.farPlane;

	lib::MTHandle<SpecularReflectionsTraceDS> traceRaysDS = graphBuilder.CreateDescriptorSet<SpecularReflectionsTraceDS>(RENDERER_RESOURCE_NAME("SpecularReflectionsTraceDS"));
	traceRaysDS->u_skyViewLUT                    = params.skyViewLUT;
	traceRaysDS->u_atmosphereParams              = atmosphereSubsystem.GetAtmosphereContext().atmosphereParamsBuffer->CreateFullView();
	traceRaysDS->u_reflectedLuminanceTexture     = reflectedLuminanceTexture;
	traceRaysDS->rayTracingDS                    = rayTracingSubsystem.GetSceneRayTracingDS();
	traceRaysDS->u_shadingNormalsTexture         = params.normalsTexture;
	traceRaysDS->u_depthTexture                  = params.depthTexture;
	traceRaysDS->u_specularAndRoughnessTexture   = params.specularColorRoughnessTexture;
	traceRaysDS->u_traceParams                   = shaderParams;
	traceRaysDS->u_integratedInScatteringTexture = fogParams.integratedInScatteringTextureView;

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

	return reflectedLuminanceTexture;
}

} // trace

namespace denoise
{

static void Denoise(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle denoisedTexture, const SpecularReflectionsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const ddgi::DDGISceneSubsystem& ddgiSceneSubsystem = renderScene.GetSceneSubsystemChecked<ddgi::DDGISceneSubsystem>();
	lib::MTHandle<ddgi::DDGISceneDS> ddgiDS = ddgiSceneSubsystem.GetDDGISceneDS();

	const RenderView& renderView = viewSpec.GetRenderView();
	const RenderSceneEntityHandle viewEntityHandle = renderView.GetViewEntity();

	SpecularReflectionsViewData& srViewData = viewEntityHandle.get_or_emplace<SpecularReflectionsViewData>();

	sr_denoiser::Denoiser::Params denoiserParams(renderView);
	denoiserParams.currentDepthTexture              = params.depthTexture;
	denoiserParams.historyDepthTexture              = params.depthHistoryTexture;
	denoiserParams.motionTexture                    = params.motionTexture;
	denoiserParams.normalsTexture                   = params.normalsTexture;
	denoiserParams.specularColorAndRoughnessTexture = params.specularColorRoughnessTexture;
	denoiserParams.ddgiSceneDS                      = ddgiDS;

	srViewData.denoiser.Denoise(graphBuilder, denoisedTexture, denoiserParams);
}

} // denoise

namespace apply
{

struct ApplySpecularReflectionsParams
{
	rg::RGTextureViewHandle luminanceTexture;
	rg::RGTextureViewHandle specularReflectionsTexture;
	rg::RGTextureViewHandle specularAndRoughnessTexture;
	rg::RGTextureViewHandle brdfIntegrationLUT;
	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle normalsTexture;
};


DS_BEGIN(ApplySpecularReflectionsDS, rg::RGDescriptorSetState<ApplySpecularReflectionsDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_luminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_specularReflectionsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_specularAndRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_brdfIntegrationLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
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
	applySpecularReflectionsDS->u_luminanceTexture                       = params.luminanceTexture;
	applySpecularReflectionsDS->u_specularReflectionsTexture             = params.specularReflectionsTexture;
	applySpecularReflectionsDS->u_specularAndRoughnessTexture            = params.specularAndRoughnessTexture;
	applySpecularReflectionsDS->u_brdfIntegrationLUT                     = params.brdfIntegrationLUT;
	applySpecularReflectionsDS->u_depthTexture                           = params.depthTexture;
	applySpecularReflectionsDS->u_normalsTexture                         = params.normalsTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Specular Reflections"),
						  applySpecularReflectionsPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(applySpecularReflectionsDS), renderView.GetRenderViewDS()));
}

} // apply


SpecularReflectionsRenderStage::SpecularReflectionsRenderStage()
{ }

void SpecularReflectionsRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		const RenderView& renderView = viewSpec.GetRenderView();

		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		const math::Vector2u halfResolution = viewSpec.GetRenderingHalfRes();

		const rg::RGTextureViewHandle shadingNormalsHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Shading Normals Half Res"),
																							 rg::TextureDef(halfResolution, viewContext.normals->GetFormat()));

		const rg::RGTextureViewHandle specularAndRoughnessHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Specular Color And Roughness Half Res"),
																							 rg::TextureDef(halfResolution, viewContext.specularAndRoughness->GetFormat()));

		DownsampledShadingTexturesParams downsampleParams;
		downsampleParams.depth                         = viewContext.depthNoJitter;
		downsampleParams.specularColorRoughness        = viewContext.specularAndRoughness;
		downsampleParams.shadingNormals                = viewContext.normals;
		downsampleParams.specularColorRoughnessHalfRes = specularAndRoughnessHalfRes;
		downsampleParams.shadingNormalsHalfRes         = shadingNormalsHalfRes;
		DownsampleShadingTextures(graphBuilder, renderView, downsampleParams);

		SpecularReflectionsParams params;
		params.resolution                    = specularAndRoughnessHalfRes->GetResolution2D();
		params.normalsTexture                = shadingNormalsHalfRes;
		params.specularColorRoughnessTexture = specularAndRoughnessHalfRes;
		params.depthTexture                  = viewContext.depthNoJitterHalfRes;
		params.depthHistoryTexture           = viewContext.historyDepthNoJitterHalfRes;
		params.motionTexture                 = viewContext.motionHalfRes;
		params.skyViewLUT                    = viewContext.skyViewLUT;

		const rg::RGTextureViewHandle specularReflectionsTexture = trace::TraceRays(graphBuilder, renderScene, viewSpec, params);
		
		denoise::Denoise(graphBuilder, renderScene, viewSpec, specularReflectionsTexture, params);

		upsampler::DepthBasedUpsampleParams upsampleParams;
		upsampleParams.debugName          = RG_DEBUG_NAME("Upsample Specular Reflections");
		// Use no jitter depth for upsampling - this should help with edge artifacts
		upsampleParams.depth              = viewContext.depth;
		upsampleParams.depthHalfRes       = viewContext.depthNoJitterHalfRes;
		upsampleParams.renderViewDS       = renderView.GetRenderViewDS();
		upsampleParams.eliminateFireflies = true;
		const rg::RGTextureViewHandle specularReflectionsFullRes = upsampler::DepthBasedUpsample(graphBuilder, specularReflectionsTexture, upsampleParams);

		const rg::RGTextureViewHandle brdfIntegrationLUT = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);

		apply::ApplySpecularReflectionsParams applyParams;
		applyParams.luminanceTexture            = viewContext.luminance;
		applyParams.specularReflectionsTexture  = specularReflectionsFullRes;
		applyParams.specularAndRoughnessTexture = viewContext.specularAndRoughness;
		applyParams.brdfIntegrationLUT          = brdfIntegrationLUT;
		applyParams.depthTexture                = viewContext.depthNoJitter;
		applyParams.normalsTexture              = viewContext.normals;
		apply::ApplySpecularReflections(graphBuilder, viewSpec, applyParams);
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
