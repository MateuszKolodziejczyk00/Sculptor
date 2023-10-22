#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class RenderView;
class ViewRenderingSpec;
class RenderViewsCollector;


class RENDER_SCENE_API ViewRenderSystem : public std::enable_shared_from_this<ViewRenderSystem>
{
public:

	ViewRenderSystem() = default;
	virtual ~ViewRenderSystem() = default;

	void Initialize(RenderView& inRenderView);
	void Deinitialize(RenderView& inRenderView);

	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector) {};

	virtual void PreRenderFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) {};

	RenderView& GetOwningView() const;

protected:

	virtual void OnInitialize(RenderView& inRenderView) {};
	virtual void OnDeinitialize(RenderView& inRenderView) {};

private:

	RenderView* m_owningView = nullptr;
};

} // spt::rsc