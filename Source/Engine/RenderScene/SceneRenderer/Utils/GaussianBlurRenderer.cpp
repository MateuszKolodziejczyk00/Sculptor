#include "GaussianBlurRenderer.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructs.h"

namespace spt::rsc
{

namespace gaussian_blur_renderer
{

BEGIN_SHADER_STRUCT(GaussianBlurConstants)
	SHADER_STRUCT_FIELD(math::Vector3u, resolution)
	SHADER_STRUCT_FIELD(Uint32,         dimention)
	SHADER_STRUCT_FIELD(Uint32,         kernelSize)
	SHADER_STRUCT_FIELD(Real32,         sigma)
	SHADER_STRUCT_FIELD(Bool,           is3DTexture)
END_SHADER_STRUCT();


DS_BEGIN(GaussianBlurDS, rg::RGDescriptorSetState<GaussianBlurDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),  u_input2D)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<math::Vector4f>),   u_output2D)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture3DBinding<math::Vector4f>),  u_input3D)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture3DBinding<math::Vector4f>),   u_output3D)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GaussianBlurConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CompileGaussianBlurPipeline()
{
	rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/GaussianBlur.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GaussianBlurCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Gaussian Blur Pipeline"), shader);
}

void ApplyGaussianBlurPass(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 dimention, const BlurPassParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dimention <= 2);

	const Bool is3DTexture = input->GetResolution().z() > 1;
	SPT_CHECK(dimention != 2 || is3DTexture);

	GaussianBlurConstants shaderConstants;
	shaderConstants.resolution  = input->GetResolution();
	shaderConstants.dimention   = dimention;
	shaderConstants.kernelSize  = params.kernelSize;
	shaderConstants.sigma       = params.sigma;
	shaderConstants.is3DTexture = is3DTexture;

	lib::MTHandle<GaussianBlurDS> gaussianBlurDS = graphBuilder.CreateDescriptorSet<GaussianBlurDS>(RENDERER_RESOURCE_NAME("GaussianBlurDS"));
	if (is3DTexture)
	{
		gaussianBlurDS->u_input3D  = input;
		gaussianBlurDS->u_output3D = output;
	}
	else
	{
		gaussianBlurDS->u_input2D  = input;
		gaussianBlurDS->u_output2D = output;
	}
	gaussianBlurDS->u_constants = shaderConstants;

	static const rdr::PipelineStateID pipeline = CompileGaussianBlurPipeline();

	const Uint32 groupSize = 128u;
	math::Vector3u dispatchSize = input->GetResolution();

	if (dimention == 0)
	{
		dispatchSize.x() = math::Utils::DivideCeil(dispatchSize.x(), groupSize);
	}
	else if (dimention == 1)
	{
		dispatchSize.y() = math::Utils::DivideCeil(dispatchSize.y(), groupSize);
	}
	else if (dimention == 2)
	{
		dispatchSize.z() = math::Utils::DivideCeil(dispatchSize.z(), groupSize);
	}

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("Gaussian Blur ({}) (dimetion {})", debugName.AsString(), dimention),
						  pipeline,
						  dispatchSize,
						  rg::BindDescriptorSets(std::move(gaussianBlurDS)));
}

rg::RGTextureViewHandle ApplyGaussianBlur2D(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, const GaussianBlur2DParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::TextureDef textureDef = rg::TextureDef(input->GetResolution(), input->GetFormat());

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurTempTexture ({})", debugName.AsString()), textureDef);
	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurOutputTexture ({})", debugName.AsString()), textureDef);

	ApplyGaussianBlurPass(graphBuilder, debugName, input, tempTexture, 0, params.horizontalPass);
	ApplyGaussianBlurPass(graphBuilder, debugName, tempTexture, outputTexture, 1, params.verticalPass);

	return outputTexture;
}

rg::RGTextureViewHandle ApplyGaussianBlur3D(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, const GaussianBlur3DParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::TextureDef textureDef = rg::TextureDef(input->GetResolution(), input->GetFormat());

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurTempTexture ({})", debugName.AsString()), textureDef);
	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurOutputTexture ({})", debugName.AsString()), textureDef);

	ApplyGaussianBlurPass(graphBuilder, debugName, input, outputTexture, 0, params.horizontalPass);
	ApplyGaussianBlurPass(graphBuilder, debugName, outputTexture, tempTexture, 1, params.verticalPass);
	ApplyGaussianBlurPass(graphBuilder, debugName, tempTexture, outputTexture, 2, params.depthPass);

	return outputTexture;
}

} // gaussian_blur_renderer

} // spt::rsc
