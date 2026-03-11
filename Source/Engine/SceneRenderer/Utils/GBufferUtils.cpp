#include "GBufferUtils.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"


namespace spt::rsc::gbuffer_utils
{

namespace oct_normals
{

BEGIN_SHADER_STRUCT(GenerateOctahedronNormalsConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(GenerateOctahedronNormalsDS, rg::RGDescriptorSetState<GenerateOctahedronNormalsDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                       u_tangentFrame)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                        u_rwOctahedronNormals)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GenerateOctahedronNormalsConstants>), u_constants)
DS_END();


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
	shaderConstants.resolution = resolution;

	lib::MTHandle<GenerateOctahedronNormalsDS> generateOctahedronNormalsDS = graphBuilder.CreateDescriptorSet<GenerateOctahedronNormalsDS>(RENDERER_RESOURCE_NAME("GenerateOctahedronNormalsDS"));
	generateOctahedronNormalsDS->u_tangentFrame        = tangentFrame;
	generateOctahedronNormalsDS->u_rwOctahedronNormals = outputTexture;
	generateOctahedronNormalsDS->u_constants           = shaderConstants;

	static const rdr::PipelineStateID generateOctahedronNormalsPipeline = CreateOctahedronNormalsPipeline();

	const math::Vector2u groupSize = math::Vector2u(8u, 8u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate Octahedron Normals"),
						  generateOctahedronNormalsPipeline,
						  math::Utils::DivideCeil(resolution, groupSize),
						  rg::BindDescriptorSets(std::move(generateOctahedronNormalsDS)));
}

} // oct_normals

} // spt::rsc::gbuffer_utils

