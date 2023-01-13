#pragma once

#include "RenderGraphMacros.h"


namespace spt::rg
{

class RENDER_GRAPH_API RenderGraphManager
{
public:

	static void OnBeginFrame();

private:

	RenderGraphManager() = default;
};

} // spt::rg