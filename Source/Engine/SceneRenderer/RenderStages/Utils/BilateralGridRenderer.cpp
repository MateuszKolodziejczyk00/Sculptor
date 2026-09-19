#include "BilateralGridRenderer.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Utils/Geometry/GeometryTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Utils/ViewRenderingSpec.h"
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

math::Vector2u GetGridTileSize()
{
	return constants::tileSize;
}

namespace builder
{

BEGIN_SHADER_STRUCT(BilateralGridRenderingConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                    inputTextureResolution)
	SHADER_STRUCT_FIELD(Real32,                            minLogLuminance)
	SHADER_STRUCT_FIELD(Real32,                            logLuminanceRange)
	SHADER_STRUCT_FIELD(Real32,                            inverseLogLuminanceRange)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, inputTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector2f>, bilateralGridTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         downsampledLogLuminanceTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileRenderBilarteralGridPipeline()
{
	rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/BilateralGrid/RenderBilateralGrid.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderBilateralGridCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Render Bilateral Grid Pipeline"), shader);
}

BilateralGridInfo RenderBilateralGrid(rg::RenderGraphBuilder& graphBuilder, const BilateralGridParams& parameters)
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

	BilateralGridRenderingConstants shaderConstants;
	shaderConstants.inputTextureResolution         = inputTexture->GetResolution2D();
	shaderConstants.minLogLuminance                = parameters.minLogLuminance;
	shaderConstants.logLuminanceRange              = parameters.logLuminanceRange;
	shaderConstants.inverseLogLuminanceRange       = 1.f / parameters.logLuminanceRange;
	shaderConstants.inputTexture                   = inputTexture;
	shaderConstants.bilateralGridTexture           = bilateralGrid;
	shaderConstants.downsampledLogLuminanceTexture = downsampledLogLuminance;

	static const rdr::PipelineStateID pipeline = CompileRenderBilarteralGridPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Bilateral Grid"),
						  pipeline,
						  bilateralGrid->GetResolution2D(),
						  rg::ShaderParams(shaderConstants));

	return { bilateralGrid, downsampledLogLuminance, GetGridTileSize() };
}

} // builder

} // bilateral_grid

} // spt::rsc
