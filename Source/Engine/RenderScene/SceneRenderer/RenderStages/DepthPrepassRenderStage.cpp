#include "DepthPrepassRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::DepthPrepass, DepthPrepassRenderStage);

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

void DepthPrepassRenderStage::BeginFrame(const RenderScene& renderScene, const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::BeginFrame(renderScene, renderView);

	PrepareDepthTextures(renderView);
}

void DepthPrepassRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	viewContext.depth        = graphBuilder.AcquireExternalTextureView(m_currentDepth);
	viewContext.depthHalfRes = graphBuilder.AcquireExternalTextureView(m_currentDepthHalfRes);

	if (m_historyDepth)
	{
		viewContext.historyDepth        = graphBuilder.AcquireExternalTextureView(m_historyDepth);
		viewContext.historyDepthHalfRes = graphBuilder.AcquireExternalTextureView(m_historyDepthHalfRes);
	}

	{
		DepthPrepassMetaData metaData;
		metaData.allowJittering = true;
		ExecuteDepthPrepass(graphBuilder, renderScene, viewSpec, stageContext, viewContext.depth, metaData);
	}
}

void DepthPrepassRenderStage::PrepareDepthTextures(const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	std::swap(m_currentDepth, m_historyDepth);
	std::swap(m_currentDepthHalfRes, m_historyDepthHalfRes);

	const math::Vector2u renderingRes = renderView.GetRenderingRes();

	if (!m_currentDepth || m_currentDepth->GetTexture()->GetResolution2D() != renderingRes)
	{
		const math::Vector2u renderingHalfRes = math::Utils::DivideCeil(renderingRes, math::Vector2u(2u, 2u));

		m_currentDepth        = utils::CreateDepthTexture(GetDepthFormat(), renderingRes, true);
		m_currentDepthHalfRes = utils::CreateDepthTexture(rhi::EFragmentFormat::R32_S_Float, renderingHalfRes, false);
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
