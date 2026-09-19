#include "AmbientOcclusionRenderStage.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RenderScene.h"
#include "Utility/Random.h"
#include "MaterialsSubsystem.h"
#include "SceneRenderer/Utils/DepthBasedUpsampler.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::AmbientOcclusion, AmbientOcclusionRenderStage);


namespace rtao
{

struct AORenderingContext
{
	const RenderScene&             renderScene;
	const ViewRenderingSpec&       viewSpec;
	visibility_denoiser::Denoiser& denoiser;
	rg::RGTextureViewHandle        depthTexture;
	rg::RGTextureViewHandle        depthTextureHalfRes;
	rg::RGTextureViewHandle        historyDepthTextureHalfRes;
	rg::RGTextureViewHandle        normalsTextureHalfRes;
	rg::RGTextureViewHandle        motionTextureHalfRes;
};

namespace trace_rays
{

BEGIN_SHADER_STRUCT(RTAOTraceRaysParams)
	SHADER_STRUCT_FIELD(math::Vector2f,                    randomSeed)
	SHADER_STRUCT_FIELD(Uint32,                            raysNumber)
	SHADER_STRUCT_FIELD(Real32,                            raysLength)
	SHADER_STRUCT_FIELD(Real32,                            raysMinHitDistance)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, normalsTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         ambientOcclusionTexture)
END_SHADER_STRUCT();


RT_PSO(AOTraceRaysPSO)
{
	RAY_GEN_SHADER("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", GenerateAmbientOcclusionRaysRTG);
	MISS_SHADERS(SHADER_ENTRY("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", GenericRTM));

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/RenderStages/AmbientOcclusion/RTAOTraceRays.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		pso = CompilePSO(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>());
	}
};


static rg::RGTextureViewHandle TraceAmbientOcclusionRays(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context, const math::Vector2u traceRaysResolution)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle traceRaysResultTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("AO Trace Rays Result"), rg::TextureDef(traceRaysResolution, rhi::EFragmentFormat::R8_UN_Float));

	RTAOTraceRaysParams params;
	params.randomSeed              = math::Vector2f(lib::rnd::Random<Real32>(), lib::rnd::Random<Real32>());
	params.raysNumber              = 1u;
	params.raysLength              = 0.25f;
	params.raysMinHitDistance      = 0.02f;
	params.depthTexture            = context.depthTextureHalfRes;
	params.normalsTexture          = context.normalsTextureHalfRes;
	params.ambientOcclusionTexture = traceRaysResultTexture;

	graphBuilder.TraceRays(RG_DEBUG_NAME("RTAO Trace Rays"),
						   AOTraceRaysPSO::pso,
						   traceRaysResolution,
						   rg::ShaderParams(params));

	return traceRaysResultTexture;
}

} // trace_rays

static rg::RGTextureViewHandle RenderAO(rg::RenderGraphBuilder& graphBuilder, const AORenderingContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "RTAO");

	const RenderView& renderView = context.viewSpec.GetRenderView();
	const math::Vector2u aoRenderingResolution = context.depthTextureHalfRes->GetResolution2D();

	const rg::RGTextureViewHandle aoHalfResTexture = trace_rays::TraceAmbientOcclusionRays(graphBuilder, context, aoRenderingResolution);

	visibility_denoiser::Denoiser::Params denoiserParams(renderView);
	denoiserParams.historyDepthTexture       = context.historyDepthTextureHalfRes;
	denoiserParams.currentDepthTexture       = context.depthTextureHalfRes;
	denoiserParams.motionTexture             = context.motionTextureHalfRes;
	denoiserParams.normalsTexture            = context.normalsTextureHalfRes;
	denoiserParams.currentFrameDefaultWeight = 0.045f;
	denoiserParams.accumulatedFramesMaxCount = 32.f;
	context.denoiser.Denoise(graphBuilder, aoHalfResTexture, denoiserParams);

	upsampler::DepthBasedUpsampleParams upsampleParams;
	upsampleParams.debugName      = RG_DEBUG_NAME("RTAO Upsample");
	upsampleParams.depth          = context.depthTexture;
	upsampleParams.depthHalfRes   = context.depthTextureHalfRes;
	upsampleParams.normalsHalfRes = context.normalsTextureHalfRes;
	return upsampler::DepthBasedUpsample(graphBuilder, aoHalfResTexture, upsampleParams);
}

} // rtao

AmbientOcclusionRenderStage::AmbientOcclusionRenderStage()
	: m_denoiser(RG_DEBUG_NAME("Ambient Occlusion Denoiser"))
{ }

void AmbientOcclusionRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	if (rdr::GPUApi::IsRayTracingEnabled())
	{
		rtao::AORenderingContext aoContext{ renderScene, viewSpec, m_denoiser };
		aoContext.normalsTextureHalfRes      = viewContext.normalsHalfRes;
		aoContext.depthTexture               = viewContext.depth;
		aoContext.depthTextureHalfRes        = viewContext.depthHalfRes;
		aoContext.historyDepthTextureHalfRes = viewContext.historyDepthHalfRes;
		aoContext.motionTextureHalfRes       = viewContext.motionHalfRes;

		viewContext.ambientOcclusion = rtao::RenderAO(graphBuilder, aoContext);
	}
}

} // spt::rsc
