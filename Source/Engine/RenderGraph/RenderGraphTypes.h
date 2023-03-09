#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rg
{

enum class ERenderGraphNodeType
{
	None,
	RenderPass,
	Dispatch,
	TraceRays,
	Transfer
};


class RenderGraphDebugName
{
public:

#if DEBUG_RENDER_GRAPH
	explicit RenderGraphDebugName(const lib::HashedString& name)
		: m_name(name)
	{ }
#endif // DEBUG_RENDER_GRAPH

	RenderGraphDebugName() = default;

	const lib::HashedString& Get() const
	{
#if DEBUG_RENDER_GRAPH
		return m_name;
#else
		static lib::HashedString name;
		return name;
#endif // DEBUG_RENDER_GRAPH
	}

private:

#if DEBUG_RENDER_GRAPH
	lib::HashedString m_name;
#endif // DEBUG_RENDER_GRAPH
};


#if DEBUG_RENDER_GRAPH

#define RG_DEBUG_NAME(name) spt::rg::RenderGraphDebugName(name)

#else

#define RG_DEBUG_NAME(name)

#endif // DEBUG_RENDER_GRAPH



} // spt::rg