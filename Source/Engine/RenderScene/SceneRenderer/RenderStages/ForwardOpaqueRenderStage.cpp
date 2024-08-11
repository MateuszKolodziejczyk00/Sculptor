#include "ForwardOpaqueRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "DepthPrepassRenderStage.h"
#include "Lights/ViewShadingInput.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::ForwardOpaque, ForwardOpaqueRenderStage);

RenderTargetFormatsDef ForwardOpaqueRenderStage::GetRenderTargetFormats(const SceneRendererSettings& sceneRendererSettings)
{
	RenderTargetFormatsDef formats;
	formats.colorRTFormats.emplace_back(SceneRendererStatics::hdrFormat);
	formats.colorRTFormats.emplace_back(SceneRendererStatics::hdrFormat);
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::RGBA16_UN_Float);
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::RGBA8_UN_Float);
	formats.depthRTFormat = DepthPrepassRenderStage::GetDepthFormat();

	return formats;
}

ForwardOpaqueRenderStage::ForwardOpaqueRenderStage()
{ }

void ForwardOpaqueRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);
	
	viewContext.luminance              = graphBuilder.CreateTextureView(RG_DEBUG_NAME("View Luminance Texture"), rg::TextureDef(texturesRes, SceneRendererStatics::hdrFormat));

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= viewContext.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef luminanceRTDef;
	luminanceRTDef.textureView		= viewContext.luminance;
	luminanceRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	luminanceRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	luminanceRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 1.f);
	renderPassDef.AddColorRenderTarget(luminanceRTDef);

	SPT_CHECK_NO_ENTRY_MSG("Forward Opaue is not supported");
	rg::RGRenderTargetDef normalsRTDef;
	normalsRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	normalsRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	normalsRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(normalsRTDef);

	rg::RGRenderTargetDef specularAndRoughnessRTDef;
	specularAndRoughnessRTDef.loadOperation  = rhi::ERTLoadOperation::Clear;
	specularAndRoughnessRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	specularAndRoughnessRTDef.clearColor     = rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(specularAndRoughnessRTDef);
	
	const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingRes();

	const ViewSpecShadingParameters& shadingParams = viewSpec.GetData().Get<ViewSpecShadingParameters>();

	graphBuilder.RenderPass(RG_DEBUG_NAME("Forward Opaque Render Pass"),
							renderPassDef,
							rg::BindDescriptorSets(shadingParams.shadingInputDS, shadingParams.shadowMapsDS),
							[renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));
							});

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
