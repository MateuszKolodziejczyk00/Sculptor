#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


class GBufferGenerationStage
{
public:

	GBufferGenerationStage();

	void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& view);
};

} // spt::rsc