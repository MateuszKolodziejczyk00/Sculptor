#include "BRDFIntegrationLUT.h"
#include "RendererUtils.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(BRDFIntegrationLUTGenerationConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, lut)
END_SHADER_STRUCT();


void GenerateBRDFIntegrationLUT(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle lut)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(lut.IsValid());

	BRDFIntegrationLUTGenerationConstants shaderConstants;
	shaderConstants.lut = lut;

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/GenerateBRDFIntegrationLUT.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateBRDFIntegrationLUTCS"));
	const rdr::PipelineStateID pipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Generate BRDF Integration LUT Pipeline"), shader);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate BRDF Integration LUT"),
						  pipeline,
						  math::Utils::DivideCeil(lut->GetResolution2D(), math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));
}

} // spt::rsc