#include "ShadowMapRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "Shadows/ShadowsRenderingTypes.h"
#include "RenderGraphBuilder.h"

namespace spt::rsc
{

rhi::EFragmentFormat ShadowMapRenderStage::GetDepthFormat()
{
	return rhi::EFragmentFormat::D16_UN_Float;
}

ShadowMapRenderStage::ShadowMapRenderStage()
{ }

void ShadowMapRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();

	const RenderSceneEntityHandle& viewEntity = viewSpec.GetRenderView().GetViewEntity();
	const ShadowMapViewComponent& shadowMapViewData = viewEntity.get<ShadowMapViewComponent>();

	const rg::RGTextureViewHandle shadowMapTextureView = graphBuilder.AcquireExternalTextureView(shadowMapViewData.shadowMapView);
	
	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= shadowMapTextureView;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(1.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Shadow Map"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[renderingRes](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingRes.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingRes));
							});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
