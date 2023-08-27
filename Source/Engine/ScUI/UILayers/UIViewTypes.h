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

	SizeType GetValue() const
	{
		return m_id;
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

	static void						SetCurrentViewContentClass(const ImGuiWindowClass& inClass);
	static const ImGuiWindowClass&	GetCurrentViewContentClass();

private:

	CurrentViewBuildingContext() = delete;

	static ImGuiWindowClass s_currentViewContentClass;
};


class ViewBuildingScope
{
public:

	explicit ViewBuildingScope(const ImGuiWindowClass& inClass)
		: m_preViewContentClass(CurrentViewBuildingContext::GetCurrentViewContentClass())
	{
		CurrentViewBuildingContext::SetCurrentViewContentClass(inClass);
	}

	~ViewBuildingScope()
	{
		CurrentViewBuildingContext::SetCurrentViewContentClass(m_preViewContentClass);
	}

private:

	ImGuiWindowClass m_preViewContentClass;
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

	void ClearViews();

	void DrawViews(const UIViewDrawParams& params);

private:

	lib::DynamicArray<lib::SharedPtr<class UIView>> m_views;
};

} // spt::scui