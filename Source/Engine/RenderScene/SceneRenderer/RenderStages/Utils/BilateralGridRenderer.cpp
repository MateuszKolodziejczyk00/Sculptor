#include "BilateralGridRenderer.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Geometry/GeometryTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/Utils/GaussianBlurRenderer.h"


namespace spt::rsc
{

namespace bilateral_grid
{

namespace constants
{
static const     math::Vector2u       tileSize = math::Vector2u::Constant(32);
static constexpr Uint32               numDepthSlices = 64;
static constexpr rhi::EFragmentFormat gridFormat = rhi::EFragmentFormat::RG16_S_Float;
} // constants

namespace builder
{

BEGIN_SHADER_STRUCT(BilateralGridRenderingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, inputTextureResolution)
	SHADER_STRUCT_FIELD(Real32,         minLogLuminance)
	SHADER_STRUCT_FIELD(Real32,         logLuminanceRange)
	SHADER_STRUCT_FIELD(Real32,         inverseLogLuminanceRange)
END_SHADER_STRUCT();


DS_BEGIN(RenderBilateralGridDS, rg::RGDescriptorSetState<RenderBilateralGridDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                    u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector2f>),                     u_bilateralGridTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                             u_downsampledLogLuminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BilateralGridRenderingConstants>), u_bilateralGridConstants)
DS_END();


static rdr::PipelineStateID CompileRenderBilarteralGridPipeline()
{
	rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/BilateralGrid/RenderBilateralGrid.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderBilateralGridCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Render Bilateral Grid Pipeline"), shader);
}

BilateralGridOutput RenderBilateralGrid(rg::RenderGraphBuilder& graphBuilder, const BilateralGridParams& parameters)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle inputTexture = parameters.inputTexture;
	SPT_CHECK(inputTexture.IsValid());

	const math::Vector2u inputResolution = inputTexture->GetResolution2D();

	const math::Vector2u downsampleLogLuminanceResolution = math::Utils::DivideCeil(inputResolution, constants::tileSize);

	math::Vector3u gridResolution{};
	gridResolution.head<2>() = downsampleLogLuminanceResolution;
	gridResolution.z()       = constants::numDepthSlices;

	const rg::RGTextureViewHandle bilateralGrid           = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Bilateral Grid"), rg::TextureDef(gridResolution, constants::gridFormat));
	const rg::RGTextureViewHandle downsampledLogLuminance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Downsampled Log Luminance"), rg::TextureDef(downsampleLogLuminanceResolution, rhi::EFragmentFormat::R16_S_Float));

	const RenderView& renderView = parameters.viewSpec.GetRenderView();

	BilateralGridRenderingConstants shaderConstants;
	shaderConstants.inputTextureResolution   = inputTexture->GetResolution2D();
	shaderConstants.minLogLuminance          = parameters.minLogLuminance;
	shaderConstants.logLuminanceRange        = parameters.logLuminanceRange;
	shaderConstants.inverseLogLuminanceRange = 1.f / parameters.logLuminanceRange;

	lib::MTHandle<RenderBilateralGridDS> bilateralGridDS = graphBuilder.CreateDescriptorSet<RenderBilateralGridDS>(RENDERER_RESOURCE_NAME("RenderBilateralGridDS"));
	bilateralGridDS->u_inputTexture                   = inputTexture;
	bilateralGridDS->u_bilateralGridTexture           = bilateralGrid;
	bilateralGridDS->u_bilateralGridConstants         = shaderConstants;
	bilateralGridDS->u_downsampledLogLuminanceTexture = downsampledLogLuminance;

	static const rdr::PipelineStateID pipeline = CompileRenderBilarteralGridPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Bilateral Grid"),
						  pipeline,
						  bilateralGrid->GetResolution2D(),
						  rg::BindDescriptorSets(std::move(bilateralGridDS),
												 renderView.GetRenderViewDS()));

	return { bilateralGrid, downsampledLogLuminance };
}

} // builder

} // bilateral_grid

} // spt::rsc
