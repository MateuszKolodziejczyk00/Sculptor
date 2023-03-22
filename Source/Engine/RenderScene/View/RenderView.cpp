#include "RenderView.h"
#include "RenderScene.h"
#include "ResourcesManager.h"

namespace spt::rsc
{

RenderView::RenderView(RenderScene& renderScene)
	: m_supportedStages(ERenderStage::None)
	, m_renderingResolution(0, 0)
	, m_enableTemporalAA(false)
#if RENDERER_DEBUG
	, m_debugFeature(EDebugFeature::None)
#endif // RENDERER_DEBUG
{
	m_viewEntity = renderScene.CreateEntity();
}

RenderView::~RenderView()
{
	m_viewEntity.destroy();
}

void RenderView::SetRenderStages(ERenderStage stages)
{
	m_supportedStages = stages;
}

void RenderView::AddRenderStages(ERenderStage stagesToAdd)
{
	lib::AddFlag(m_supportedStages, stagesToAdd);
}

void RenderView::RemoveRenderStages(ERenderStage stagesToRemove)
{
	lib::RemoveFlag(m_supportedStages, stagesToRemove);
}

ERenderStage RenderView::GetSupportedStages() const
{
	return m_supportedStages;
}

void RenderView::SetRenderingResolution(const math::Vector2u& resolution)
{
	m_renderingResolution = resolution;
}

const math::Vector2u& RenderView::GetRenderingResolution() const
{
	return m_renderingResolution;
}

const RenderSceneEntityHandle& RenderView::GetViewEntity() const
{
	return m_viewEntity;
}

const lib::SharedPtr<RenderViewDS>& RenderView::GetRenderViewDS() const
{
	return m_renderViewDS;
}

lib::SharedRef<RenderViewDS> RenderView::GetRenderViewDSRef() const
{
	SPT_CHECK(!!GetRenderViewDS());
	return lib::Ref(GetRenderViewDS());
}

void RenderView::SetTemporalAAEnabled(Bool enable)
{
	m_enableTemporalAA = enable;
}

Bool RenderView::IsTemporalAAEnabled() const
{
	return m_enableTemporalAA;
}

#if RENDERER_DEBUG
void RenderView::SetDebugFeature(EDebugFeature::Type debugFeature)
{
	m_debugFeature = debugFeature;
}

EDebugFeature::Type RenderView::GetDebugFeature() const
{
	return m_debugFeature;
}

Bool RenderView::IsAnyDebugFeatureEnabled() const
{
	return m_debugFeature != EDebugFeature::None;
}
#endif // RENDERER_DEBUG

void RenderView::OnBeginRendering()
{
	SPT_PROFILER_FUNCTION();

	CachePrevFrameRenderingData();
	UpdateViewRenderingData(GetRenderingResolution());
	UpdateCullingData();

	CreateRenderViewDS();
}

void RenderView::CreateRenderViewDS()
{
	SPT_PROFILER_FUNCTION();

	RenderViewData renderViewData;
	renderViewData.renderingResolution = GetRenderingResolution();

#if RENDERER_DEBUG
	renderViewData.debugFeatureIndex = GetDebugFeature();
#endif // RENDERER_DEBUG

	m_renderViewDS = rdr::ResourcesManager::CreateDescriptorSetState<RenderViewDS>(RENDERER_RESOURCE_NAME("RenderViewDS"));
	m_renderViewDS->u_prevFrameSceneView	= GetPrevFrameRenderingData();
	m_renderViewDS->u_sceneView				= GetViewRenderingData();
	m_renderViewDS->u_cullingData			= GetCullingData();
	m_renderViewDS->u_viewRenderingParams	= renderViewData;
}

} // spt::rsc
