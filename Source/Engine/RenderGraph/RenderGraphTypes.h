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

	lib::String AsString() const
	{
		return Get().ToString();
	}

private:

#if DEBUG_RENDER_GRAPH
	lib::HashedString m_name;
#endif // DEBUG_RENDER_GRAPH
};


#if DEBUG_RENDER_GRAPH

#define RG_DEBUG_NAME(name) spt::rg::RenderGraphDebugName(name)
#define RG_DEBUG_NAME_FORMATTED(...) spt::rg::RenderGraphDebugName(std::format(__VA_ARGS__))

#else

#define RG_DEBUG_NAME(name)
#define RG_DEBUG_NAME_FORMATTED(...)

#endif // DEBUG_RENDER_GRAPH


struct WorkloadResolution
{
	WorkloadResolution(Uint32 groupsNum)
		: m_groupsNum(groupsNum, 1u, 1u)
	{ }

	WorkloadResolution(math::Vector2u groupsNum)
		: m_groupsNum(groupsNum.x(), groupsNum.y(), 1u)
	{ }

	WorkloadResolution(const math::Vector3u& groupsNum)
		: m_groupsNum(groupsNum)
	{ }

	const math::Vector3u& AsVector() const
	{
		return m_groupsNum;
	}

private:

	math::Vector3u m_groupsNum;
};

} // spt::rg