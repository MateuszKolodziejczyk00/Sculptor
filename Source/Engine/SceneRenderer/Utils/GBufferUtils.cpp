#include "GBufferUtils.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"


namespace spt::rsc::gbuffer_utils
{

namespace oct_normals
{

BEGIN_SHADER_STRUCT(GenerateOctahedronNormalsConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, tangentFrame)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, rwOctahedronNormals)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateOctahedronNormalsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/GBuffer/GenerateOctahedronNormals.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateOctahedronNormalsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateOctahedronNormalsPipeline"), shader);
}


void GenerateOctahedronNormals(rg::RenderGraphBuilder& graphBuilder, const GBuffer& gBuffer, rg::RGTextureViewHandle outputTexture)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle tangentFrame = gBuffer[GBuffer::Texture::TangentFrame];

	SPT_CHECK(tangentFrame.IsValid());
	SPT_CHECK(tangentFrame->GetResolution2D() == outputTexture->GetResolution2D());

	const math::Vector2u resolution = tangentFrame->GetResolution2D();

	GenerateOctahedronNormalsConstants shaderConstants;
	shaderConstants.resolution          = resolution;
	shaderConstants.tangentFrame        = tangentFrame;
	shaderConstants.rwOctahedronNormals = outputTexture;

	static const rdr::PipelineStateID generateOctahedronNormalsPipeline = CreateOctahedronNormalsPipeline();

	const math::Vector2u groupSize = math::Vector2u(8u, 8u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate Octahedron Normals"),
						  generateOctahedronNormalsPipeline,
						  math::Utils::DivideCeil(resolution, groupSize),
						  rg::ShaderParams(shaderConstants));
}

} // oct_normals

} // spt::rsc::gbuffer_utils

