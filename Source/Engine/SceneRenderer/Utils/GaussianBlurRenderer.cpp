#include "GaussianBlurRenderer.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructs.h"

namespace spt::rsc
{

namespace gaussian_blur_renderer
{

BEGIN_SHADER_STRUCT(GaussianBlurConstants)
	SHADER_STRUCT_FIELD(math::Vector3u,                    resolution)
	SHADER_STRUCT_FIELD(Uint32,                            dimention)
	SHADER_STRUCT_FIELD(Uint32,                            kernelSize)
	SHADER_STRUCT_FIELD(Real32,                            sigma)
	SHADER_STRUCT_FIELD(Bool,                              is3DTexture)
	SHADER_STRUCT_FIELD(Bool,                              useTonemappedValues)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, input2D)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, output2D)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, input3D)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>, output3D)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileGaussianBlurPipeline()
{
	rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/GaussianBlur.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GaussianBlurCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Gaussian Blur Pipeline"), shader);
}

void ApplyGaussianBlurPass(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 dimention, Bool useTonemappedValues, const BlurPassParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dimention <= 2);

	const Bool is3DTexture = input->GetResolution().z() > 1;
	SPT_CHECK(dimention != 2 || is3DTexture);

	GaussianBlurConstants shaderConstants;
	shaderConstants.resolution          = input->GetResolution();
	shaderConstants.dimention           = dimention;
	shaderConstants.kernelSize          = params.kernelSize;
	shaderConstants.sigma               = params.sigma;
	shaderConstants.is3DTexture         = is3DTexture;
	shaderConstants.useTonemappedValues = useTonemappedValues;

	if (is3DTexture)
	{
		shaderConstants.input3D  = input;
		shaderConstants.output3D = output;
	}
	else
	{
		shaderConstants.input2D  = input;
		shaderConstants.output2D = output;
	}

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
						  rg::ShaderParams(shaderConstants));
}

rg::RGTextureViewHandle ApplyGaussianBlur2D(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, const GaussianBlur2DParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::TextureDef textureDef = rg::TextureDef(input->GetResolution(), input->GetFormat());

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurTempTexture ({})", debugName.AsString()), textureDef);
	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurOutputTexture ({})", debugName.AsString()), textureDef);

	ApplyGaussianBlurPass(graphBuilder, debugName, input, tempTexture, 0, params.useTonemappedValues, params.horizontalPass);
	ApplyGaussianBlurPass(graphBuilder, debugName, tempTexture, outputTexture, 1, params.useTonemappedValues, params.verticalPass);

	return outputTexture;
}

rg::RGTextureViewHandle ApplyGaussianBlur3D(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, const GaussianBlur3DParams& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::TextureDef textureDef = rg::TextureDef(input->GetResolution(), input->GetFormat());

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurTempTexture ({})", debugName.AsString()), textureDef);
	const rg::RGTextureViewHandle outputTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("GaussianBlurOutputTexture ({})", debugName.AsString()), textureDef);

	ApplyGaussianBlurPass(graphBuilder, debugName, input, outputTexture, 0, params.useTonemappedValues, params.horizontalPass);
	ApplyGaussianBlurPass(graphBuilder, debugName, outputTexture, tempTexture, 1, params.useTonemappedValues, params.verticalPass);
	ApplyGaussianBlurPass(graphBuilder, debugName, tempTexture, outputTexture, 2, params.useTonemappedValues, params.depthPass);

	return outputTexture;
}

} // gaussian_blur_renderer

} // spt::rsc
