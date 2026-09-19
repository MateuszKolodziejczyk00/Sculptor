#include "DOFRenderer.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "View/RenderView.h"


namespace spt::rsc
{

namespace dof
{

namespace coc_generation
{
 
BEGIN_SHADER_STRUCT(DOFShaderParameters)
	SHADER_STRUCT_FIELD(Real32,                            nearFieldBegin)
	SHADER_STRUCT_FIELD(Real32,                            nearFieldEnd)
	SHADER_STRUCT_FIELD(Real32,                            farFieldBegin)
	SHADER_STRUCT_FIELD(Real32,                            farFieldEnd)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, cocTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileGenerateCoCPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFGenerateCoC.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFGenerateCoCCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateCoCPipeline"), shader);
}

static rg::RGTextureViewHandle DOFGenerateCoC(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = params.linearColorTexture->GetResolution();

	const rg::RGTextureViewHandle cocTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("COC Texture"), rg::TextureDef(resolution, rhi::EFragmentFormat::RG8_UN_Float));

	DOFShaderParameters shaderParams;
	shaderParams.nearFieldEnd	= params.focalPlane - (params.fullFocusRange * 0.5f);
	shaderParams.nearFieldBegin = shaderParams.nearFieldEnd - params.nearFocusIncreaseRange;
	shaderParams.farFieldBegin	= params.focalPlane + (params.fullFocusRange * 0.5f);
	shaderParams.farFieldEnd	= shaderParams.farFieldBegin + params.farFocusIncreaseRange;
	shaderParams.depthTexture	= params.depthTexture;
	shaderParams.cocTexture		= cocTexture;

	static const rdr::PipelineStateID geenrateCoCPipeline = CompileGenerateCoCPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Generate CoC"),
						  geenrateCoCPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(shaderParams));

	return cocTexture;
}

} // coc_generation


namespace downsample
{

BEGIN_SHADER_STRUCT(DOFNearCoCBlurParams)
	SHADER_STRUCT_FIELD(Bool,                              isHorizontal)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, cocTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         cocTextureBlurred)
END_SHADER_STRUCT();


enum class ENearCoCBlurType
{
	Max,
	Average
};


static rdr::PipelineStateID CompileNearFieldCoCBlur(ENearCoCBlurType blurType)
{
	sc::ShaderCompilationSettings compilationSettings;
	switch (blurType)
	{
	case ENearCoCBlurType::Max:
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("DOF_BLUR_OP", "1"));
		break;
	case ENearCoCBlurType::Average:
		compilationSettings.AddMacroDefinition(sc::MacroDefinition("DOF_BLUR_OP", "2"));
		break;
	}

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFNearCoCBlur.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFNearCoCBlurCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DOFNearCoCBlurPipeline"), shader);
}


static rg::RGTextureViewHandle DOFBlurNearFieldCoC(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle cocTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = cocTexture->GetResolution();

	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("COC Texture Blurred"), rg::TextureDef(resolution, rhi::EFragmentFormat::R8_UN_Float));
	const rg::RGTextureViewHandle cocTextureBlurred = graphBuilder.CreateTextureView(RG_DEBUG_NAME("COC Texture Blurred"), rg::TextureDef(resolution, rhi::EFragmentFormat::R8_UN_Float));

	static const rdr::PipelineStateID maxBlurPipeline = CompileNearFieldCoCBlur(ENearCoCBlurType::Max);

	// Horizontal max filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;
		params.cocTexture			= cocTexture;
		params.cocTextureBlurred	= tempTexture;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Max Blur Horizontal"),
							  maxBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::ShaderParams(params));
	}

	// Vertical max filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;
		params.cocTexture			= tempTexture;
		params.cocTextureBlurred	= cocTextureBlurred;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Max Blur Horizontal"),
							  maxBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::ShaderParams(params));
	}

	static const rdr::PipelineStateID averageBlurPipeline = CompileNearFieldCoCBlur(ENearCoCBlurType::Average);

	// Horizontal average filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;
		params.cocTexture			= cocTextureBlurred;
		params.cocTextureBlurred	= tempTexture;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Average Blur Horizontal"),
							  averageBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::ShaderParams(params));
	}

	// Vertical average filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;
		params.cocTexture			= tempTexture;
		params.cocTextureBlurred	= cocTextureBlurred;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Average Blur Horizontal"),
							  averageBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::ShaderParams(params));
	}

	return cocTextureBlurred;
}


BEGIN_SHADER_STRUCT(DOFDownsampleParams)
	SHADER_STRUCT_FIELD(math::Vector2f,                    inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2u,                    inputResolution)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, linearColorTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, cocTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, cocHalfTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, linearColorHalfTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, linearColorMulFarHalfTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileDownsampleDOFPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFDownsample.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFDownsampleCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DOFDownsamplePipeline"), shader);
}


struct DOFDownsampleResult
{
	rg::RGTextureViewHandle cocHalfTexture;
	rg::RGTextureViewHandle	cocNearTextureBlurred;
	rg::RGTextureViewHandle	linearColorHalfTexture;
	rg::RGTextureViewHandle	linearColorMulFarHalfTexture;
};


static DOFDownsampleResult DOFDownsample(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params, rg::RGTextureViewHandle cocTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = params.linearColorTexture->GetResolution();
	const math::Vector3u halfResolution = math::Utils::DivideCeil(resolution, math::Vector3u(2u, 2u, 2u));

	const rg::RGTextureViewHandle halfResCoC = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Half Resolution CoC"), rg::TextureDef(halfResolution, rhi::EFragmentFormat::RG8_UN_Float));

	const rg::TextureDef halfLinearColorDef(halfResolution, rhi::EFragmentFormat::B10G11R11_U_Float);
	const rg::RGTextureViewHandle halfLinearColor = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Half Resolution Linear Color"), halfLinearColorDef);
	const rg::RGTextureViewHandle halfLinearColorMulFar = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Half Resolution Linear Color Mul Far"), halfLinearColorDef);

	DOFDownsampleParams downsampleParams;
	downsampleParams.inputPixelSize		= math::Vector2f(1.f / resolution.x(), 1.f / resolution.y());
	downsampleParams.inputResolution	= math::Vector2u(resolution.x(), resolution.y());
	downsampleParams.linearColorTexture				= params.linearColorTexture;
	downsampleParams.cocTexture						= cocTexture;
	downsampleParams.cocHalfTexture					= halfResCoC;
	downsampleParams.linearColorHalfTexture			= halfLinearColor;
	downsampleParams.linearColorMulFarHalfTexture	= halfLinearColorMulFar;

	static const rdr::PipelineStateID downsamplePipeline = CompileDownsampleDOFPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Downsample"),
						  downsamplePipeline,
						  math::Utils::DivideCeil(halfResolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(downsampleParams));

	const rg::RGTextureViewHandle cocNearTextureBlurred = DOFBlurNearFieldCoC(graphBuilder, halfResCoC);

	DOFDownsampleResult result;
	result.cocHalfTexture				= halfResCoC;
	result.cocNearTextureBlurred		= cocNearTextureBlurred;
	result.linearColorHalfTexture		= halfLinearColor;
	result.linearColorMulFarHalfTexture	= halfLinearColorMulFar;

	return result;
}

} // downsample


namespace computation
{

BEGIN_SHADER_STRUCT(DOFFillPassParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, nearFieldDOFTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, farFieldDOFTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, cocTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         cocNearBlurredTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, nearFieldFilledDOFTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, farFieldFilledDOFTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileDOFFillPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFFill.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFFillCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DOFFillPipeline"), shader);
}


struct DOFFillResult
{
	rg::RGTextureViewHandle nearFieldFilledDOF;
	rg::RGTextureViewHandle farFieldFilledDOF;
};


static DOFFillResult DOFFill(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle nearFieldDOFTexture, rg::RGTextureViewHandle farFieldDOFTexture, const downsample::DOFDownsampleResult& downsampleResult)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = nearFieldDOFTexture->GetResolution();

	const rg::TextureDef dofFilledTexturesDef(resolution, rhi::EFragmentFormat::B10G11R11_U_Float);
	const rg::RGTextureViewHandle nearFieldFilledDOFTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Near Field Filled DOF"), dofFilledTexturesDef);
	const rg::RGTextureViewHandle farFieldFilledDOFTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Far Field Filled DOF"), dofFilledTexturesDef);

	DOFFillPassParams fillPassParams;
	fillPassParams.nearFieldDOFTexture			= nearFieldDOFTexture;
	fillPassParams.farFieldDOFTexture			= farFieldDOFTexture;
	fillPassParams.cocTexture					= downsampleResult.cocHalfTexture;
	fillPassParams.cocNearBlurredTexture			= downsampleResult.cocNearTextureBlurred;
	fillPassParams.nearFieldFilledDOFTexture		= nearFieldFilledDOFTexture;
	fillPassParams.farFieldFilledDOFTexture		= farFieldFilledDOFTexture;

	static const rdr::PipelineStateID fillPipeline = CompileDOFFillPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Fill"),
						  fillPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(fillPassParams));

	DOFFillResult result;
	result.nearFieldFilledDOF	= nearFieldFilledDOFTexture;
	result.farFieldFilledDOF	= farFieldFilledDOFTexture;

	return result;
}


BEGIN_SHADER_STRUCT(DOComputationPassParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, linearColorTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, linearColorMulFarTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, cocTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         cocNearBlurredTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, nearFieldDOFTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, farFieldDOFTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileDOFComputationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFComputation.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFComputationCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DOFComputationPipeline"), shader);
}


struct DOFComputationResult
{
	rg::RGTextureViewHandle farFieldDOF;
	rg::RGTextureViewHandle nearFieldDOF;
};


static DOFComputationResult DOFComputation(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params, const downsample::DOFDownsampleResult& downsampleResult)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = downsampleResult.linearColorHalfTexture->GetResolution();

	const rg::RGTextureViewHandle nearFieldDOFTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Near Field DOF"), rg::TextureDef(resolution, rhi::EFragmentFormat::B10G11R11_U_Float));
	const rg::RGTextureViewHandle farFieldDOFTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Far Field DOF"), rg::TextureDef(resolution, rhi::EFragmentFormat::B10G11R11_U_Float));

	DOComputationPassParams computationPassParams;
	computationPassParams.linearColorTexture			= downsampleResult.linearColorHalfTexture;
	computationPassParams.linearColorMulFarTexture	= downsampleResult.linearColorMulFarHalfTexture;
	computationPassParams.cocTexture					= downsampleResult.cocHalfTexture;
	computationPassParams.cocNearBlurredTexture		= downsampleResult.cocNearTextureBlurred;
	computationPassParams.nearFieldDOFTexture		= nearFieldDOFTexture;
	computationPassParams.farFieldDOFTexture			= farFieldDOFTexture;

	static const rdr::PipelineStateID computationPipeline = CompileDOFComputationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Computation"),
						  computationPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(computationPassParams));

	const DOFFillResult fillResult = DOFFill(graphBuilder, nearFieldDOFTexture, farFieldDOFTexture, downsampleResult);

	DOFComputationResult result;
	result.farFieldDOF	= fillResult.farFieldFilledDOF;
	result.nearFieldDOF = fillResult.nearFieldFilledDOF;

	return result;
}

} // computation


namespace composite
{

BEGIN_SHADER_STRUCT(DOFCompositeParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, nearFieldDOFTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, farFieldDOFTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, cocTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         nearCoCBlurredTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, resultTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileDOFCompositePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFComposite.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFCompositeCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DOFCompositePipeline"), shader);
}


static void DOFComposite(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params, const downsample::DOFDownsampleResult& downsampleResult, const computation::DOFComputationResult& computationResult)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = params.linearColorTexture->GetResolution();

	DOFCompositeParams compositePassParams;
	compositePassParams.nearFieldDOFTexture		= computationResult.nearFieldDOF;
	compositePassParams.farFieldDOFTexture		= computationResult.farFieldDOF;
	compositePassParams.cocTexture				= downsampleResult.cocHalfTexture;
	compositePassParams.nearCoCBlurredTexture	= downsampleResult.cocNearTextureBlurred;
	compositePassParams.resultTexture			= params.linearColorTexture;

	static const rdr::PipelineStateID compositePipeline = CompileDOFCompositePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Composite"),
						  compositePipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(compositePassParams));
}

} // composite

void RenderGatherBasedDOF(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle cocTexture = coc_generation::DOFGenerateCoC(graphBuilder, params);

	const downsample::DOFDownsampleResult downsampleResult = downsample::DOFDownsample(graphBuilder, params, cocTexture);

	computation::DOFComputationResult computationResult = computation::DOFComputation(graphBuilder, params, downsampleResult);

	composite::DOFComposite(graphBuilder, params, downsampleResult, computationResult);
}

} // dof

} // spt::rsc
