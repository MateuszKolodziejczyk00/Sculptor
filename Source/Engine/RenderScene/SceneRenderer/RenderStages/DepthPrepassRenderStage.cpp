#include "DepthPrepassRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"

namespace spt::rsc
{

struct SceneViewDepthDataComponent
{
	lib::SharedPtr<rdr::TextureView> currentDepthTexture;
	lib::SharedPtr<rdr::TextureView> currentDepthNoJitterTexture;
	lib::SharedPtr<rdr::TextureView> currentDepthNoJitterTextureHalfRes;
	lib::SharedPtr<rdr::TextureView> historyDepthTexture;
	lib::SharedPtr<rdr::TextureView> historyDepthNoJitterTexture;
	lib::SharedPtr<rdr::TextureView> historyDepthNoJitterTextureHalfRes;

	Bool HasSeparateNoJitterTextures() const
	{
		return !!currentDepthNoJitterTexture && !!historyDepthNoJitterTexture;
	}
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

	const Bool wantsSeparateNoJitterTextures = renderView.GetAntiAliasingMode() == EAntiAliasingMode::TemporalAA;

	std::swap(viewDepthData->currentDepthTexture, viewDepthData->historyDepthTexture);
	std::swap(viewDepthData->currentDepthNoJitterTextureHalfRes, viewDepthData->historyDepthNoJitterTextureHalfRes);

	if (viewDepthData->currentDepthTexture != viewDepthData->currentDepthNoJitterTexture)
	{
		std::swap(viewDepthData->currentDepthNoJitterTexture, viewDepthData->historyDepthNoJitterTexture);
	}

	const math::Vector2u renderingRes = renderView.GetRenderingResolution();

	if (!viewDepthData->currentDepthTexture || viewDepthData->currentDepthTexture->GetTexture()->GetResolution2D() != renderingRes)
	{
		const math::Vector2u renderingHalfRes = math::Utils::DivideCeil(renderingRes, math::Vector2u(2u, 2u));

		viewDepthData->currentDepthTexture                = utils::CreateDepthTexture(GetDepthFormat(), renderingRes, true);
		viewDepthData->currentDepthNoJitterTextureHalfRes = utils::CreateDepthTexture(rhi::EFragmentFormat::R32_S_Float, renderingHalfRes, false);
		
		viewDepthData->currentDepthNoJitterTexture = wantsSeparateNoJitterTextures
			                                       ? utils::CreateDepthTexture(GetDepthFormat(), renderingRes, true)
			                                       : viewDepthData->currentDepthTexture;
	}

	DepthPrepassData& depthPrepassData = viewSpec.GetData().Create<DepthPrepassData>();

	depthPrepassData.depth         = graphBuilder.AcquireExternalTextureView(viewDepthData->currentDepthTexture);
	depthPrepassData.depthNoJitter = wantsSeparateNoJitterTextures
		                           ? graphBuilder.AcquireExternalTextureView(viewDepthData->currentDepthNoJitterTexture)
		                           : depthPrepassData.depth;

	depthPrepassData.depthNoJitterHalfRes = graphBuilder.AcquireExternalTextureView(viewDepthData->currentDepthNoJitterTextureHalfRes);

	if (viewDepthData->historyDepthTexture)
	{
		depthPrepassData.historyDepthNoJitter        = graphBuilder.AcquireExternalTextureView(viewDepthData->historyDepthNoJitterTexture);
		depthPrepassData.historyDepthNoJitterHalfRes = graphBuilder.AcquireExternalTextureView(viewDepthData->historyDepthNoJitterTextureHalfRes);
	}

	{
		DepthPrepassMetaData metaData;
		metaData.allowJittering = true;
		ExecuteDepthPrepass(graphBuilder, renderScene, viewSpec, stageContext, depthPrepassData.depth, metaData);
	}

	if (wantsSeparateNoJitterTextures)
	{
		DepthPrepassMetaData metaData;
		metaData.allowJittering = false;
		ExecuteDepthPrepass(graphBuilder, renderScene, viewSpec, stageContext, depthPrepassData.depthNoJitter, metaData);
	}
}

void DepthPrepassRenderStage::ExecuteDepthPrepass(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, rg::RGTextureViewHandle depthTarget, const DepthPrepassMetaData& metaData)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = depthTarget->GetResolution2D();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), resolution);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthTarget;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);
	
	graphBuilder.RenderPass(RG_DEBUG_NAME("Depth Prepass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[resolution](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));
							});

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext, metaData);

}

} // spt::rsc
