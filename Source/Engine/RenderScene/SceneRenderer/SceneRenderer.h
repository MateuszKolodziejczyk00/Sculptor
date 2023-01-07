#pragma once

#include "SculptorCoreTypes.h"


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
};

} // spt::rsc