#include "SRTemporalAccumulation.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"
#include "RenderScene.h"
#include "EngineFrame.h"
#include "SceneRenderer/RenderStages/Utils/SharcGICache.h"
#include "SRDenoiserTypes.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRTemporalAccumulationParams)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,               specularHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               historySpecularHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,               diffuseHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,               historyDiffuseHistoryLengthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               historyDepthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       motionTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       normalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       historyNormalsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               historyRoughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               roughnessTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,       specularTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,       diffuseTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, rwSpecularY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<RTSphericalBasisType>, rwDiffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,       rwDiffSpecCoCg)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,               rwSpecHitDist)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       lightDirection)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, historySpecularY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<RTSphericalBasisType>, historyDiffuseY_SH2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,       historyDiffSpecCoCg)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>,       rwSpecularTemporalVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       specularHistoryTemporalVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>,       rwSpecularFastHistoryTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,       specularFastHistoryTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>,       rwDiffuseTemporalVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,       diffuseHistoryTemporalVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>,       rwDiffuseFastHistoryTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>,       diffuseFastHistoryTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,       baseColorMetallicTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline(const TemporalAccumulationParameters& params)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("USE_STABLE_BLENDS", params.enableStableHistoryBlend ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRTemporalAccumulation.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRTemporalAccumulationCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Temporal Accumulation Pipeline"), shader);
}


void ApplyTemporalAccumulation(rg::RenderGraphBuilder& graphBuilder, const TemporalAccumulationParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.currentSpecularTexture->GetResolution2D();

	SRTemporalAccumulationParams shaderParams;
	shaderParams.specularHistoryLengthTexture           = params.specularHistoryLengthTexture;
	shaderParams.historySpecularHistoryLengthTexture    = params.historySpecularHistoryLengthTexture;
	shaderParams.diffuseHistoryLengthTexture            = params.diffuseHistoryLengthTexture;
	shaderParams.historyDiffuseHistoryLengthTexture     = params.historyDiffuseHistoryLengthTexture;
	shaderParams.historyDepthTexture                    = params.historyDepthTexture;
	shaderParams.depthTexture                           = params.currentDepthTexture;
	shaderParams.motionTexture                          = params.motionTexture;
	shaderParams.normalsTexture                         = params.normalsTexture;
	shaderParams.historyNormalsTexture                  = params.historyNormalsTexture;
	shaderParams.historyRoughnessTexture                = params.historyRoughnessTexture;
	shaderParams.roughnessTexture                       = params.currentRoughnessTexture;
	shaderParams.specularTexture                        = params.currentSpecularTexture;
	shaderParams.diffuseTexture                         = params.currentDiffuseTexture;
	shaderParams.rwSpecularY_SH2                        = params.specularY_SH2;
	shaderParams.rwDiffuseY_SH2                         = params.diffuseY_SH2;
	shaderParams.rwDiffSpecCoCg                         = params.diffSpecCoCg;
	shaderParams.rwSpecHitDist                          = params.specHitDist;
	shaderParams.lightDirection                         = params.lightDirection;
	shaderParams.historySpecularY_SH2                   = params.historySpecularY_SH2;
	shaderParams.historyDiffuseY_SH2                    = params.historyDiffuseY_SH2;
	shaderParams.historyDiffSpecCoCg                    = params.historyDiffSpecCoCg;
	shaderParams.rwSpecularTemporalVarianceTexture      = params.temporalVarianceSpecularTexture;
	shaderParams.specularHistoryTemporalVarianceTexture = params.historyTemporalVarianceSpecularTexture;
	shaderParams.rwSpecularFastHistoryTexture           = params.fastHistorySpecularTexture;
	shaderParams.specularFastHistoryTexture             = params.fastHistorySpecularOutputTexture;
	shaderParams.rwDiffuseTemporalVarianceTexture       = params.temporalVarianceDiffuseTexture;
	shaderParams.diffuseHistoryTemporalVarianceTexture  = params.historyTemporalVarianceDiffuseTexture;
	shaderParams.rwDiffuseFastHistoryTexture            = params.fastHistoryDiffuseTexture;
	shaderParams.diffuseFastHistoryTexture              = params.fastHistoryDiffuseOutputTexture;
	shaderParams.baseColorMetallicTexture               = params.baseColorMetallic;

	const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline(params);

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: SR Temporal Accumulation", params.name.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderParams));
}

} // spt::rsc::sr_denoiser
