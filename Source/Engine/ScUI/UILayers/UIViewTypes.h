#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"
#include "ImGui/SculptorImGui.h"


namespace spt::scui
{

using ImGuiID = unsigned int;


struct SCUI_API UIViewID
{
public:

	static UIViewID GenerateID();

	UIViewID()
		: m_id(idxNone<SizeType>)
	{ }

	UIViewID(const UIViewID& rhs) = default;
	UIViewID& operator=(const UIViewID& rhs) = default;

	Bool operator==(const UIViewID& rhs) const
	{
		return m_id == rhs.m_id;
	}

	Bool IsValid() const
	{
		return m_id != idxNone<SizeType>;
	}

	void Reset()
	{
		m_id = idxNone<SizeType>;
	}

private:

	explicit UIViewID(SizeType id)
		: m_id(id)
	{ }

	SizeType m_id;
};


class SCUI_API CurrentViewBuildingContext
{
public:

	static void		SetCurrentViewDockspaceID(ImGuiID inDockspaceID);
	static ImGuiID	GetCurrentViewDockspaceID();

private:

	CurrentViewBuildingContext() = delete;

	static ImGuiID s_currentWindowDockspaceID;
};


class ViewBuildingScope
{
public:

	explicit ViewBuildingScope(ImGuiID id)
		: m_prevDockspaceID(CurrentViewBuildingContext::GetCurrentViewDockspaceID())
	{
		CurrentViewBuildingContext::SetCurrentViewDockspaceID(id);
	}

	~ViewBuildingScope()
	{
		CurrentViewBuildingContext::SetCurrentViewDockspaceID(m_prevDockspaceID);
	}

private:

	ImGuiID m_prevDockspaceID;
};


struct UIViewDrawParams
{
	UIViewDrawParams() = default;

	ImGuiWindowClass parentClass;
};


enum class EViewDrawResultActions
{
	None		= 0,
	Close		= BIT(0)
};


class UIViewsContainer
{
public:

	UIViewsContainer() = default;

	void AddView(lib::SharedRef<class UIView> view);
	
	void RemoveView(UIViewID viewID);
	void RemoveView(const lib::SharedPtr<UIView>& view);

	void DrawViews(const UIViewDrawParams& params);

private:

	lib::DynamicArray<lib::SharedPtr<class UIView>> m_views;
};

} // spt::scui