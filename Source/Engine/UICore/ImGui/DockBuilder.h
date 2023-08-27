#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorImGui.h"


namespace spt::ui
{

enum class ESplit
{
	Horizontal,
	Vertical
};


template<typename TFirst, typename TSecond>
class Split
{
public:

	Split(ESplit direction, Real32 splitRatio, const TFirst& first, const TSecond& second)
		: m_first(first)
		, m_second(second)
		, m_direction(direction)
		, m_splitRatio(splitRatio)
	{ }

	void Execute(ImGuiID parentNodeID) const
	{
		const ImGuiDir dir = m_direction == ESplit::Horizontal ? ImGuiDir_Left : ImGuiDir_Up;

		ImGuiID firstNode, secondNode;
		ImGui::DockBuilderSplitNode(parentNodeID, dir, m_splitRatio, OUT &firstNode, OUT &secondNode);

		m_first.Execute(firstNode);
		m_second.Execute(secondNode);
	}

private:

	TFirst	m_first;
	TSecond	m_second;

	ESplit	m_direction;
	Real32	m_splitRatio;
};


class DockWindow
{
public:

	explicit DockWindow(const lib::HashedString& windowName)
		: m_windowName(windowName)
	{ }

	void Execute(ImGuiID parentNodeID) const
	{
		ImGui::DockBuilderDockWindow(m_windowName.GetData(), parentNodeID);
	}

private:

	lib::HashedString	m_windowName;
};


template<typename TOperators>
class Build
{
public:

	Build(ImGuiID nodeID, const TOperators& operators)
	{
		operators.Execute(nodeID);

		ImGui::DockBuilderFinish(nodeID);
	}
};


// template deduction rules
template<typename TFirst, typename TSecond>
Split(ESplit direction, Real32 splitRatio, const TFirst& first, const TSecond& second)->Split<TFirst, TSecond>;

template<typename TOperators>
Build(ImGuiID nodeID, const TOperators& operators)->Build<TOperators>;


inline Bool ShouldBuildDock(ImGuiID id)
{
	return !ImGui::DockBuilderGetNode(id)->IsSplitNode();
}

} // spt::ui