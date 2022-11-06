#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/Delegate.h"


namespace spt::rdr
{
class CommandRecorder;
} // spt::rdr


namespace spt::rg
{

class RGAccess
{
	
};


class RenderGraphNode
{
public:

	RenderGraphNode() = default;



private:

	lib::Delegate<void(rdr::CommandRecorder& /*cmdRecorder*/)> m_callable;
};


class RENDER_GRAPH_API RenderGraph
{
public:

	RenderGraph();

private:

};

} // spt::rg
