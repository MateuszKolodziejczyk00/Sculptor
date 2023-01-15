#include "GBufferGenerationStage.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"

namespace spt::rsc
{

GBufferGenerationStage::GBufferGenerationStage()
{ }

void GBufferGenerationStage::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	RenderStageEntries& viewStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::GBufferGenerationStage);

	RenderStageExecutionContext stageContext(ERenderStage::GBufferGenerationStage);

	viewStageEntries.GetPreRenderStageDelegate().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);

	const rhi::RHIAllocationInfo gBufferAllocationInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::ETextureUsage gBufferColorTexturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::ColorRT);

	GBufferData& gBuffer = viewSpec.GetData().Create<GBufferData>();
	
	rhi::TextureDefinition depthDef;
	depthDef.resolution = texturesRes;
	depthDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::DepthSetncilRT);
	depthDef.format = rhi::EFragmentFormat::D32_S_Float;
	gBuffer.depth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GBuffer Depth"), depthDef, gBufferAllocationInfo);

	rhi::TextureDefinition testDef;
	testDef.resolution = texturesRes;
	testDef.usage = gBufferColorTexturesUsage;
	testDef.format = rhi::EFragmentFormat::RGBA8_UN_Float;
	gBuffer.testColor = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GBuffer Color"), testDef, gBufferAllocationInfo);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView						= gBuffer.depth;
	depthRTDef.loadOperation					= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation					= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor.asDepthStencil.depth	= 0.f;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef colorRTDef;
	colorRTDef.textureView						= gBuffer.testColor;
	colorRTDef.loadOperation					= rhi::ERTLoadOperation::Clear;
	colorRTDef.storeOperation					= rhi::ERTStoreOperation::Store;
	colorRTDef.clearColor.asFloat[0]			= 1.f;
	colorRTDef.clearColor.asFloat[1]			= 0.f;
	colorRTDef.clearColor.asFloat[2]			= 0.f;
	colorRTDef.clearColor.asFloat[3]			= 1.f;
	renderPassDef.AddColorRenderTarget(colorRTDef);

	graphBuilder.AddRenderPass(RG_DEBUG_NAME("GBuffer Generation Render Pass"),
							   renderPassDef,
							   rg::EmptyDescriptorSets(),
							   [] (const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) {});

	viewStageEntries.GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
	
	viewStageEntries.GetPostRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
