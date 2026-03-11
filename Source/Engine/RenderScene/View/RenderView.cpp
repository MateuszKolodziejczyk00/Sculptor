#include "RenderView.h"
#include "RenderScene.h"
#include "ResourcesManager.h"
#include "Techniques/TemporalAA/TemporalAATypes.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderView ====================================================================================

RenderView::RenderView()
	: m_outputResolution(0u, 0u)
{
}

RenderView::~RenderView()
{
}

void RenderView::BeginFrame(math::Vector2u renderingRes)
{
	SPT_PROFILER_FUNCTION();

	CachePrevFrameRenderingData();
	UpdateViewRenderingData(renderingRes);
	UpdateCullingData();
}

void RenderView::SetOutputRes(const math::Vector2u& resolution)
{
	m_outputResolution = resolution;
}

const math::Vector2u& RenderView::GetOutputRes() const
{
	return m_outputResolution;
}

math::Vector3u RenderView::GetOutputRes3D() const
{
	return math::Vector3u(m_outputResolution.x(), m_outputResolution.y(), 1u);
}

lib::Blackboard& RenderView::GetBlackboard()
{
	return m_blackboard;
}

const lib::Blackboard& RenderView::GetBlackboard() const
{
	return m_blackboard;
}

} // spt::rsc
