#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "View/ViewRenderingSpec.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class RenderView;

class RENDER_SCENE_API SceneRenderer
{
public:

	SceneRenderer();

	void Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view);

private:

	lib::DynamicArray<ViewRenderingSpec> CollectRenderViews(RenderScene& scene, RenderView& view) const;
};

} // spt::rsc