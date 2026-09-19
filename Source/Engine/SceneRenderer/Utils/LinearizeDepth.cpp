#include "LinearizeDepth.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "View/RenderView.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(LinearizeDepthConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,            resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,            invResolution)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, depth)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, rwLinearDepth)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateLinearizeDepthPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/LinearizeDepth.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "LinearizeDepthCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("LinearizeDepthPipeline"), shader);
}


rg::RGTextureViewHandle ExecuteLinearizeDepth(rg::RenderGraphBuilder& graphBuilder, const RenderView& renderView, rg::RGTextureViewHandle depth)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(depth.IsValid());

	const math::Vector2u resolution = depth->GetResolution2D();

	const rg::RGTextureViewHandle linearDepthTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Linear Depth Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::R32_S_Float));

	LinearizeDepthConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.depth         = depth;
	shaderConstants.rwLinearDepth = linearDepthTexture;

	static const rdr::PipelineStateID linearizeDepthPipeline = CreateLinearizeDepthPipeline();

	const math::Vector2u groupSize = math::Vector2u(8u, 8u) * 2u;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Linearize Depth"),
						  linearizeDepthPipeline,
						  math::Utils::DivideCeil(resolution, groupSize),
						  rg::ShaderParams(shaderConstants));

	return linearDepthTexture;
}

} // spt::rsc
