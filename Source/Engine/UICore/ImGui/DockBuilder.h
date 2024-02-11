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

		ImGuiContext& g = *GImGui;
		ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(&g, parentNodeID);
		if(dockNode->IsSplitNode())
		{
			firstNode  = dockNode->ChildNodes[0]->ID;
			secondNode = dockNode->ChildNodes[1]->ID;
		}
		else
		{
			ImGui::DockBuilderSplitNode(parentNodeID, dir, m_splitRatio, OUT &firstNode, OUT &secondNode);
		}

		m_first.Execute(firstNode);
		m_second.Execute(secondNode);
	}

private:

	TFirst  m_first;
	TSecond m_second;

	ESplit m_direction;
	Real32 m_splitRatio;
};


// Useful if we want to call execute with non-const or non-temporary objects
template<typename TPrimitive>
class DockPrimitive
{
public:

	explicit DockPrimitive(TPrimitive& primitive)
		: m_primitive(primitive)
	{ }

	void Execute(ImGuiID parentNodeID) const
	{
		m_primitive.Execute(parentNodeID);
	}

private:

	TPrimitive& m_primitive;
};


class DockWindow
{
public:

	explicit DockWindow(const lib::HashedString& windowName)
		: m_windowName(windowName)
	{ }

	void Execute(ImGuiID parentNodeID) const
	{
		ImGuiWindow* window           = ImGui::FindWindowByName(m_windowName.GetData());
		ImGuiWindowSettings* settings = window ? ImGui::FindWindowSettings(window->ID) : nullptr;

		if (!settings)
		{
			ImGui::DockBuilderDockWindow(m_windowName.GetData(), parentNodeID);
		}
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

template<typename TPrimitive>
DockPrimitive(TPrimitive& primitive)->DockPrimitive<TPrimitive>;

template<typename TOperators>
Build(ImGuiID nodeID, const TOperators& operators)->Build<TOperators>;

} // spt::ui