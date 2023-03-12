#include "RenderView.h"
#include "RenderScene.h"
#include "ResourcesManager.h"

namespace spt::rsc
{

RenderView::RenderView(RenderScene& renderScene)
	: m_supportedStages(ERenderStage::None)
	, m_renderingResolution(0, 0)
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

void RenderView::OnBeginRendering()
{
	SPT_PROFILER_FUNCTION();

	CachePrevFrameRenderingData();
	UpdateViewRenderingData();
	UpdateCullingData();

	CreateRenderViewDS();
}

void RenderView::CreateRenderViewDS()
{
	SPT_PROFILER_FUNCTION();

	RenderViewData renderViewData;
	renderViewData.renderingResolution = GetRenderingResolution();

	m_renderViewDS = rdr::ResourcesManager::CreateDescriptorSetState<RenderViewDS>(RENDERER_RESOURCE_NAME("RenderViewDS"));
	m_renderViewDS->u_prevFrameSceneView	= GetPrevFrameRenderingData();
	m_renderViewDS->u_sceneView				= GetViewRenderingData();
	m_renderViewDS->u_cullingData			= GetCullingData();
	m_renderViewDS->u_viewRenderingParams	= renderViewData;
}

} // spt::rsc
