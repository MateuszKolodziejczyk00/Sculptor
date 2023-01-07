#pragma once

#include "SculptorCoreTypes.h"
#include "View/RenderViewEntryPoints.h"


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

	void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, RenderView& view);

private:

	lib::DynamicArray<RenderViewEntryPoints> CollectRenderViews(const RenderScene& scene, RenderView& view) const;
};

} // spt::rsc