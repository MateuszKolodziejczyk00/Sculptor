#include "ViewRenderSystem.h"


namespace spt::rsc
{

void ViewRenderSystem::Initialize(RenderView& inRenderView)
{
	SPT_CHECK(!m_owningView);

	m_owningView = &inRenderView;

	OnInitialize(inRenderView);
}

void ViewRenderSystem::Deinitialize(RenderView& inRenderView)
{
	OnDeinitialize(inRenderView);

	SPT_CHECK(m_owningView == &inRenderView);

	m_owningView = nullptr;
}

RenderView& ViewRenderSystem::GetOwningView() const
{
	SPT_CHECK(!!m_owningView);

	return *m_owningView;
}

} // spt::rsc
