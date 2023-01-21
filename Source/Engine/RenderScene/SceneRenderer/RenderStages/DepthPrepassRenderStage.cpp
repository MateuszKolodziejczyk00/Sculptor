#include "DepthPrepassRenderStage.h"

#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "SceneRenderer/SceneRendererTypes.h"

namespace spt::rsc
{

DepthPrepassRenderStage::DepthPrepassRenderStage()
{

}

void DepthPrepassRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);

	const rhi::RHIAllocationInfo depthTextureAllocationInfo(rhi::EMemoryUsage::GPUOnly);

	DepthPrepassResult& depthPrepassResult = viewSpec.GetData().Create<DepthPrepassResult>();
	
	rhi::TextureDefinition depthDef;
	depthDef.resolution = texturesRes;
	depthDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::DepthSetncilRT);
	depthDef.format = rhi::EFragmentFormat::D32_S_Float;
	depthPrepassResult.depth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DepthTexture"), depthDef, depthTextureAllocationInfo);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthPrepassResult.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Depth Prepass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) {});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
