#include "LinearizeDepth.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "View/RenderView.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(LinearizeDepthConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
END_SHADER_STRUCT();


DS_BEGIN(LinearizeDepthDS, rg::RGDescriptorSetState<LinearizeDepthDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depth)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_rwLinearDepth)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LinearizeDepthConstants>),                 u_constants)
DS_END();


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

	lib::MTHandle<LinearizeDepthDS> linearizeDepthDS = graphBuilder.CreateDescriptorSet<LinearizeDepthDS>(RENDERER_RESOURCE_NAME("LinearizeDepthDS"));
	linearizeDepthDS->u_depth         = depth;
	linearizeDepthDS->u_rwLinearDepth = linearDepthTexture;
	linearizeDepthDS->u_constants     = shaderConstants;

	static const rdr::PipelineStateID linearizeDepthPipeline = CreateLinearizeDepthPipeline();

	const math::Vector2u groupSize = math::Vector2u(8u, 8u) * 2u;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Linearize Depth"),
						  linearizeDepthPipeline,
						  math::Utils::DivideCeil(resolution, groupSize),
						  rg::BindDescriptorSets(std::move(linearizeDepthDS)));

	return linearDepthTexture;
}

} // spt::rsc
