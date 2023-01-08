#pragma once

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

class SceneRenderer abstract
{
public:

	SceneRenderer();

	void Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view);

private:

	lib::DynamicArray<ViewRenderingSpec> CollectRenderViews(RenderScene& scene, RenderView& view) const;
};

} // spt::rsc