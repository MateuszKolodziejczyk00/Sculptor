#include "VisibilityBufferRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "StaticMeshes/StaticMeshRenderSceneSubsystem.h"
#include "Utils/hiZRenderer.h"
#include "Utils/VisibilityBuffer/MaterialDepthRenderer.h"
#include "Geometry/MaterialsRenderer.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::VisibilityBuffer, VisibilityBufferRenderStage);

namespace utils
{

static lib::SharedPtr<rdr::TextureView> CreateDepthTexture(rdr::RendererResourceName name, rhi::EFragmentFormat format, math::Vector2u resolution, Bool isRenderTarget)
{
	rhi::TextureDefinition depthDef;
	depthDef.resolution	= resolution;
	depthDef.usage		= lib::Flags(rhi::ETextureUsage::SampledTexture, (isRenderTarget ? rhi::ETextureUsage::DepthSetncilRT : rhi::ETextureUsage::StorageTexture));
	depthDef.format		= format;

#if !SPT_RELEASE
	lib::AddFlag(depthDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE

	return rdr::ResourcesManager::CreateTextureView(name, depthDef, rhi::EMemoryUsage::GPUOnly);
}

} // utils


rhi::EFragmentFormat VisibilityBufferRenderStage::GetDepthFormat()
{
	return rhi::EFragmentFormat::D32_S_Float;
}

VisibilityBufferRenderStage::VisibilityBufferRenderStage()
{ }

void VisibilityBufferRenderStage::BeginFrame(const RenderScene& renderScene, const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::BeginFrame(renderScene, renderView);

	PrepareDepthTextures(renderView);
}

void VisibilityBufferRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ExecuteVisbilityBufferRendering(graphBuilder, renderScene, viewSpec, stageContext);
}

void VisibilityBufferRenderStage::PrepareDepthTextures(const RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	std::swap(m_currentDepthTexture, m_historyDepthTexture);
	std::swap(m_currentHiZTexture, m_historyHiZTexture);
	std::swap(m_currentDepthHalfRes, m_historyDepthHalfRes);

	const math::Vector2u renderingRes     = renderView.GetRenderingRes();
	const math::Vector2u renderingHalfRes = math::Utils::DivideCeil(renderingRes, math::Vector2u(2, 2));

	if (!m_currentDepthTexture || m_currentDepthTexture->GetResolution2D() != renderingRes)
	{
		m_currentDepthTexture = utils::CreateDepthTexture(RENDERER_RESOURCE_NAME("Depth Texture"), GetDepthFormat(), renderingRes, true);
		m_currentDepthHalfRes = utils::CreateDepthTexture(RENDERER_RESOURCE_NAME("Depth Texture Half Res"), rhi::EFragmentFormat::R32_S_Float, renderingHalfRes, false);
	}

	const HiZ::HiZSizeInfo hiZSizeInfo = HiZ::ComputeHiZSizeInfo(renderingRes);
	if (!m_currentHiZTexture || m_currentHiZTexture->GetResolution2D() != hiZSizeInfo.resolution)
	{
		rhi::TextureDefinition hiZDef;
		hiZDef.resolution = hiZSizeInfo.resolution;
		hiZDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
#if !SPT_RELEASE
		lib::AddFlag(hiZDef.usage, rhi::ETextureUsage::TransferSource);
#endif // !SPT_RELEASE
		hiZDef.format     = rhi::EFragmentFormat::R32_S_Float;
		hiZDef.mipLevels  = hiZSizeInfo.mipLevels;
		m_currentHiZTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("HiZ Texture"), hiZDef, rhi::EMemoryUsage::GPUOnly);
	}
}

void VisibilityBufferRenderStage::ExecuteVisbilityBufferRendering(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = viewSpec.GetRenderingRes();
	
	ShadingViewContext& shadingContext = viewSpec.GetShadingViewContext();

	const StaticMeshRenderSceneSubsystem& staticMeshPrimsSystem = renderScene.GetSceneSubsystemChecked<StaticMeshRenderSceneSubsystem>();
	const GeometryPassDataCollection& cachedGeometryPassData    = staticMeshPrimsSystem.GetCachedGeometryPassData();
	
	const rg::RGTextureViewHandle depth        = graphBuilder.AcquireExternalTextureView(m_currentDepthTexture);
	const rg::RGTextureViewHandle hiZ          = graphBuilder.AcquireExternalTextureView(m_currentHiZTexture);
	const rg::RGTextureViewHandle historyDepth = m_historyDepthTexture ? graphBuilder.AcquireExternalTextureView(m_historyDepthTexture) : nullptr;
	const rg::RGTextureViewHandle historyHiZ   = m_historyHiZTexture ? graphBuilder.AcquireExternalTextureView(m_historyHiZTexture) : nullptr;

	const rg::RGTextureViewHandle visibilityTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Visibility Texture"), rg::TextureDef(viewSpec.GetRenderingRes(), rhi::EFragmentFormat::R32_U_Int));

	VisPassParams visPassParams{ viewSpec, cachedGeometryPassData };
	visPassParams.historyHiZ        = historyHiZ;
	visPassParams.depth             = depth;
	visPassParams.hiZ               = hiZ;
	visPassParams.visibilityTexture = visibilityTexture;

	const GeometryPassResult geometryPassesResult = m_geometryVisRenderer.RenderVisibility(graphBuilder, visPassParams);

	const rg::RGTextureViewHandle materialDepth = material_depth_renderer::CreateMaterialDepthTexture(graphBuilder, resolution);

	material_depth_renderer::MaterialDepthParameters materialDepthParams;
	materialDepthParams.visibilityTexture = visibilityTexture;
	materialDepthParams.visibleMeshlets   = geometryPassesResult.visibleMeshlets;

	material_depth_renderer::RenderMaterialDepth(graphBuilder, materialDepthParams, materialDepth);

	const rg::RGTextureViewHandle materialTiles = material_depth_tiles_renderer::CreateMaterialDepthTilesTexture(graphBuilder, resolution);

	material_depth_tiles_renderer::RenderMaterialDepthTiles(graphBuilder, materialDepth, materialTiles);

	shadingContext.gBuffer.Create(graphBuilder, resolution);

	materials_renderer::MaterialsPassParams materialsPassParams(viewSpec, cachedGeometryPassData);
	materialsPassParams.materialDepthTileSize    = material_depth_tiles_renderer::GetMaterialDepthTileSize();
	materialsPassParams.materialDepthTexture     = materialDepth;
	materialsPassParams.materialDepthTileTexture = materialTiles;
	materialsPassParams.depthTexture             = depth;
	materialsPassParams.visibleMeshletsBuffer    = geometryPassesResult.visibleMeshlets;
	materialsPassParams.visibilityTexture        = visibilityTexture;

	materials_renderer::ExecuteMaterialsPass(graphBuilder, materialsPassParams);

	shadingContext.depth               = depth;
	shadingContext.depthHalfRes        = graphBuilder.AcquireExternalTextureView(m_currentDepthHalfRes);

	shadingContext.historyDepth        = historyDepth;
	if (m_historyDepthHalfRes)
	{
		shadingContext.historyDepthHalfRes = graphBuilder.AcquireExternalTextureView(m_historyDepthHalfRes);
	}

	shadingContext.hiZ = hiZ;

	DepthCullingParams depthCullingParams;
	depthCullingParams.hiZResolution = hiZ->GetResolution2D().cast<Real32>();

	lib::MTHandle<DepthCullingDS> depthCullingDS = graphBuilder.CreateDescriptorSet<DepthCullingDS>(RENDERER_RESOURCE_NAME("DepthCullingDS"));
	depthCullingDS->u_hiZTexture         = hiZ;
	depthCullingDS->u_depthCullingParams = depthCullingParams;
	shadingContext.depthCullingDS = std::move(depthCullingDS);
}

} // spt::rsc
