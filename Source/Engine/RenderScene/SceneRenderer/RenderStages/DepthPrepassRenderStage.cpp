#include "DepthPrepassRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "SceneRenderer/SceneRendererTypes.h"

namespace spt::rsc
{

struct SceneViewDepthDataComponent
{
	lib::SharedPtr<rdr::TextureView> currentDepthTexture;
	lib::SharedPtr<rdr::TextureView> prevFrameDepthTexture;
	lib::SharedPtr<rdr::TextureView> currentDepthTextureHalfRes;
	lib::SharedPtr<rdr::TextureView> prevFrameDepthTextureHalfRes;
};


namespace utils
{

static lib::SharedPtr<rdr::TextureView> CreateDepthTexture(rhi::EFragmentFormat format, math::Vector2u resolution, Bool isRenderTarget)
{
	rhi::TextureDefinition depthDef;
	depthDef.resolution	= resolution;
	depthDef.usage		= lib::Flags(rhi::ETextureUsage::SampledTexture, (isRenderTarget ? rhi::ETextureUsage::DepthSetncilRT : rhi::ETextureUsage::StorageTexture));
	depthDef.format		= format;

#if !SPT_RELEASE
	lib::AddFlag(depthDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE

	const lib::SharedRef<rdr::Texture> depthTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("Depth Texture"), depthDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(isRenderTarget ? rhi::ETextureAspect::Depth : rhi::ETextureAspect::Color);
	return depthTexture->CreateView(RENDERER_RESOURCE_NAME("Depth Texture View"), viewDefinition);
}

} // utils


rhi::EFragmentFormat DepthPrepassRenderStage::GetDepthFormat()
{
	return rhi::EFragmentFormat::D32_S_Float;
}

DepthPrepassRenderStage::DepthPrepassRenderStage()
{ }

void DepthPrepassRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	const RenderSceneEntityHandle viewEntity = renderView.GetViewEntity();

	SceneViewDepthDataComponent* viewDepthData = viewEntity.try_get<SceneViewDepthDataComponent>();
	if (!viewDepthData)
	{
		viewDepthData = &viewEntity.emplace<SceneViewDepthDataComponent>();
	}
	SPT_CHECK(!!viewDepthData);

	std::swap(viewDepthData->currentDepthTexture, viewDepthData->prevFrameDepthTexture);
	std::swap(viewDepthData->currentDepthTextureHalfRes, viewDepthData->prevFrameDepthTextureHalfRes);

	const math::Vector2u renderingRes = renderView.GetRenderingResolution();

	if (!viewDepthData->currentDepthTexture || viewDepthData->currentDepthTexture->GetTexture()->GetResolution2D() != renderingRes)
	{
		const math::Vector2u renderinHalfRes = math::Utils::DivideCeil(renderingRes, math::Vector2u(2u, 2u));
		viewDepthData->currentDepthTexture			= utils::CreateDepthTexture(GetDepthFormat(), renderingRes, true);
		viewDepthData->currentDepthTextureHalfRes	= utils::CreateDepthTexture(rhi::EFragmentFormat::R32_S_Float, renderinHalfRes, false);
	}

	DepthPrepassData& depthPrepassData = viewSpec.GetData().Create<DepthPrepassData>();

	depthPrepassData.depth			= graphBuilder.AcquireExternalTextureView(viewDepthData->currentDepthTexture);
	depthPrepassData.depthHalfRes	= graphBuilder.AcquireExternalTextureView(viewDepthData->currentDepthTextureHalfRes);

	if (viewDepthData->prevFrameDepthTexture)
	{
		depthPrepassData.prevFrameDepth			= graphBuilder.AcquireExternalTextureView(viewDepthData->prevFrameDepthTexture);
		depthPrepassData.prevFrameDepthHalfRes	= graphBuilder.AcquireExternalTextureView(viewDepthData->prevFrameDepthTextureHalfRes);
	}

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthPrepassData.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);
	
	const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();
	
	graphBuilder.RenderPass(RG_DEBUG_NAME("Depth Prepass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));
							});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
