#include "RenderView.h"

namespace spt::rsc
{

RenderView::RenderView(RenderSceneEntityHandle viewEntity)
	: m_supportedStages(ERenderStage::None)
	, m_renderingResolution(0, 0)
	, m_viewEntity(viewEntity)
{ }

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

} // spt::rsc
