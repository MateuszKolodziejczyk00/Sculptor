#include "ViewRenderSystem.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewRenderSystem ==============================================================================

ViewRenderSystem::ViewRenderSystem(RenderView& inRenderView)
	: m_owningView(inRenderView)
{
}

RenderView& ViewRenderSystem::GetOwningView() const
{
	return m_owningView;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewRenderSystemsAPI ==========================================================================

ViewRenderSystemsAPI& ViewRenderSystemsAPI::Get()
{
	static ViewRenderSystemsAPI instance;
	return instance;
}

ViewRenderSystem* ViewRenderSystemsAPI::CallConstructor(EViewRenderSystem type, lib::MemoryArena& arena, RenderView& owningView) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.constructor(arena, owningView);
}

void ViewRenderSystemsAPI::CallDestructor(EViewRenderSystem type, ViewRenderSystem& system) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.destructor(system);
}

void ViewRenderSystemsAPI::CallInitialize(EViewRenderSystem type, ViewRenderSystem& system, RenderView& owningView) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.initialize(system, owningView);
}

void ViewRenderSystemsAPI::CallDeinitialize(EViewRenderSystem type, ViewRenderSystem& system, RenderView& owningView) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.deinitialize(system, owningView);
}

void ViewRenderSystemsAPI::CallCollectRenderViews(EViewRenderSystem type, ViewRenderSystem& system, const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.collectRenderViews(system, renderScene, owningView, viewsCollector);
}

void ViewRenderSystemsAPI::CallPrepareRenderView(EViewRenderSystem type, ViewRenderSystem& system, ViewRenderingSpec& viewSpec) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.prepareRenderView(system, viewSpec);
}

void ViewRenderSystemsAPI::CallBeginFrame(EViewRenderSystem type, ViewRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) const
{
	const SystemEntry& systemEntry = GetSystemEntry(type);
	return systemEntry.beginFrame(system, graphBuilder, renderScene, viewSpec);
}

} // spt::rsc
