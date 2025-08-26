#include "DOFRenderer.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
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
	SHADER_STRUCT_FIELD(Real32, nearFieldBegin)
	SHADER_STRUCT_FIELD(Real32, nearFieldEnd)
	
	SHADER_STRUCT_FIELD(Real32, farFieldBegin)
	SHADER_STRUCT_FIELD(Real32, farFieldEnd)
END_SHADER_STRUCT();


DS_BEGIN(DOFGenerateCoCDS, rg::RGDescriptorSetState<DOFGenerateCoCDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DOFShaderParameters>),                     u_params)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_cocTexture)
DS_END();


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

	lib::MTHandle<DOFGenerateCoCDS> cocDS = graphBuilder.CreateDescriptorSet<DOFGenerateCoCDS>(RENDERER_RESOURCE_NAME("DOFGenerateCoCDS"));
	cocDS->u_params					= shaderParams;
	cocDS->u_depthTexture			= params.depthTexture;
	cocDS->u_cocTexture				= cocTexture;

	static const rdr::PipelineStateID geenrateCoCPipeline = CompileGenerateCoCPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Generate CoC"),
						  geenrateCoCPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(cocDS,
												 params.viewDS));

	return cocTexture;
}

} // coc_generation


namespace downsample
{

BEGIN_SHADER_STRUCT(DOFNearCoCBlurParams)
	SHADER_STRUCT_FIELD(Bool, isHorizontal)
END_SHADER_STRUCT();


DS_BEGIN(DOFNearCoCBlurDS, rg::RGDescriptorSetState<DOFNearCoCBlurDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_cocTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_cocTextureBlurred)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DOFNearCoCBlurParams>),                    u_params)
DS_END();


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

		lib::MTHandle<DOFNearCoCBlurDS> cocDS = graphBuilder.CreateDescriptorSet<DOFNearCoCBlurDS>(RENDERER_RESOURCE_NAME("DOFNearCoCBlurDS"));
		cocDS->u_cocTexture			= cocTexture;
		cocDS->u_cocTextureBlurred	= tempTexture;
		cocDS->u_params				= params;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Max Blur Horizontal"),
							  maxBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::BindDescriptorSets(cocDS));
	}

	// Vertical max filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;

		lib::MTHandle<DOFNearCoCBlurDS> cocDS = graphBuilder.CreateDescriptorSet<DOFNearCoCBlurDS>(RENDERER_RESOURCE_NAME("DOFNearCoCBlurDS"));
		cocDS->u_cocTexture			= tempTexture;
		cocDS->u_cocTextureBlurred	= cocTextureBlurred;
		cocDS->u_params				= params;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Max Blur Horizontal"),
							  maxBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::BindDescriptorSets(cocDS));
	}

	static const rdr::PipelineStateID averageBlurPipeline = CompileNearFieldCoCBlur(ENearCoCBlurType::Average);

	// Horizontal average filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;

		lib::MTHandle<DOFNearCoCBlurDS> cocDS = graphBuilder.CreateDescriptorSet<DOFNearCoCBlurDS>(RENDERER_RESOURCE_NAME("DOFNearCoCBlurDS"));
		cocDS->u_cocTexture			= cocTextureBlurred;
		cocDS->u_cocTextureBlurred	= tempTexture;
		cocDS->u_params				= params;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Average Blur Horizontal"),
							  averageBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::BindDescriptorSets(cocDS));
	}

	// Vertical average filter
	{
		DOFNearCoCBlurParams params;
		params.isHorizontal = true;

		lib::MTHandle<DOFNearCoCBlurDS> cocDS = graphBuilder.CreateDescriptorSet<DOFNearCoCBlurDS>(RENDERER_RESOURCE_NAME("DOFNearCoCBlurDS"));
		cocDS->u_cocTexture			= tempTexture;
		cocDS->u_cocTextureBlurred	= cocTextureBlurred;
		cocDS->u_params				= params;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Near Field CoC Average Blur Horizontal"),
							  averageBlurPipeline,
							  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
							  rg::BindDescriptorSets(cocDS));
	}

	return cocTextureBlurred;
}


BEGIN_SHADER_STRUCT(DOFDownsampleParams)
	SHADER_STRUCT_FIELD(math::Vector2f, inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2u, inputResolution)
END_SHADER_STRUCT();


DS_BEGIN(DOFDownsampleDS, rg::RGDescriptorSetState<DOFDownsampleDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DOFDownsampleParams>),                     u_params)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_linearColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_cocTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_cocHalfTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_linearColorHalfTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_linearColorMulFarHalfTexture)
DS_END();


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

	lib::MTHandle<DOFDownsampleDS> downsampleDS = graphBuilder.CreateDescriptorSet<DOFDownsampleDS>(RENDERER_RESOURCE_NAME("DOFDownsampleDS"));
	downsampleDS->u_params							= downsampleParams;
	downsampleDS->u_linearColorTexture				= params.linearColorTexture;
	downsampleDS->u_cocTexture						= cocTexture;
	downsampleDS->u_cocHalfTexture					= halfResCoC;
	downsampleDS->u_linearColorHalfTexture			= halfLinearColor;
	downsampleDS->u_linearColorMulFarHalfTexture	= halfLinearColorMulFar;

	static const rdr::PipelineStateID downsamplePipeline = CompileDownsampleDOFPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Downsample"),
						  downsamplePipeline,
						  math::Utils::DivideCeil(halfResolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(downsampleDS));

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

DS_BEGIN(DOFFillPassDS, rg::RGDescriptorSetState<DOFFillPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_nearFieldDOFTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_farFieldDOFTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_cocTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_cocNearBlurredTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_nearFieldFilledDOFTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_farFieldFilledDOFTexture)
DS_END();


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

	const lib::MTHandle<DOFFillPassDS> fillPassDS = graphBuilder.CreateDescriptorSet<DOFFillPassDS>(RENDERER_RESOURCE_NAME("DOFFillPassDS"));
	fillPassDS->u_nearFieldDOFTexture			= nearFieldDOFTexture;
	fillPassDS->u_farFieldDOFTexture			= farFieldDOFTexture;
	fillPassDS->u_cocTexture					= downsampleResult.cocHalfTexture;
	fillPassDS->u_cocNearBlurredTexture			= downsampleResult.cocNearTextureBlurred;
	fillPassDS->u_nearFieldFilledDOFTexture		= nearFieldFilledDOFTexture;
	fillPassDS->u_farFieldFilledDOFTexture		= farFieldFilledDOFTexture;

	static const rdr::PipelineStateID fillPipeline = CompileDOFFillPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Fill"),
						  fillPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(fillPassDS));

	DOFFillResult result;
	result.nearFieldFilledDOF	= nearFieldFilledDOFTexture;
	result.farFieldFilledDOF	= farFieldFilledDOFTexture;

	return result;
}


DS_BEGIN(DOComputationPassDS, rg::RGDescriptorSetState<DOComputationPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_linearColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_linearColorMulFarTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_cocTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_cocNearBlurredTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_nearFieldDOFTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_farFieldDOFTexture)
DS_END();


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

	const lib::MTHandle<DOComputationPassDS> computationPassDS = graphBuilder.CreateDescriptorSet<DOComputationPassDS>(RENDERER_RESOURCE_NAME("DOComputationPassDS"));
	computationPassDS->u_linearColorTexture			= downsampleResult.linearColorHalfTexture;
	computationPassDS->u_linearColorMulFarTexture	= downsampleResult.linearColorMulFarHalfTexture;
	computationPassDS->u_cocTexture					= downsampleResult.cocHalfTexture;
	computationPassDS->u_cocNearBlurredTexture		= downsampleResult.cocNearTextureBlurred;
	computationPassDS->u_nearFieldDOFTexture		= nearFieldDOFTexture;
	computationPassDS->u_farFieldDOFTexture			= farFieldDOFTexture;

	static const rdr::PipelineStateID computationPipeline = CompileDOFComputationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Computation"),
						  computationPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(computationPassDS));

	const DOFFillResult fillResult = DOFFill(graphBuilder, nearFieldDOFTexture, farFieldDOFTexture, downsampleResult);

	DOFComputationResult result;
	result.farFieldDOF	= fillResult.farFieldFilledDOF;
	result.nearFieldDOF = fillResult.nearFieldFilledDOF;

	return result;
}

} // computation


namespace composite
{

DS_BEGIN(DOFCompositeDS, rg::RGDescriptorSetState<DOFCompositeDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_nearFieldDOFTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_farFieldDOFTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_cocTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_nearCoCBlurredTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),								u_resultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
DS_END();


static rdr::PipelineStateID CompileDOFCompositePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/DOF/DOFComposite.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DOFCompositeCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DOFCompositePipeline"), shader);
}


static void DOFComposite(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params, const downsample::DOFDownsampleResult& downsampleResult, const computation::DOFComputationResult& computationResult)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = params.linearColorTexture->GetResolution();

	const lib::MTHandle<DOFCompositeDS> compositePassDS = graphBuilder.CreateDescriptorSet<DOFCompositeDS>(RENDERER_RESOURCE_NAME("DOCCompositePassDS"));
	compositePassDS->u_nearFieldDOFTexture		= computationResult.nearFieldDOF;
	compositePassDS->u_farFieldDOFTexture		= computationResult.farFieldDOF;
	compositePassDS->u_cocTexture				= downsampleResult.cocHalfTexture;
	compositePassDS->u_nearCoCBlurredTexture	= downsampleResult.cocNearTextureBlurred;
	compositePassDS->u_resultTexture			= params.linearColorTexture;

	static const rdr::PipelineStateID compositePipeline = CompileDOFCompositePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DOF Composite"),
						  compositePipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(compositePassDS));
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
