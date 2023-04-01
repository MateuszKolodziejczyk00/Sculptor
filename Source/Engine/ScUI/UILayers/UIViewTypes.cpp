#include "UILayers/UIViewTypes.h"
#include "UIView.h"

namespace spt::scui
{

UIViewID UIViewID::GenerateID()
{
	static SizeType id = 1;
	return UIViewID(id++);
}

ImGuiWindowClass CurrentViewBuildingContext::s_currentViewContentClass{};

void CurrentViewBuildingContext::SetCurrentViewContentClass(const ImGuiWindowClass& inClass)
{
	s_currentViewContentClass = inClass;
}

const ImGuiWindowClass& CurrentViewBuildingContext::GetCurrentViewContentClass()
{
	return s_currentViewContentClass;
}

void UIViewsContainer::AddView(lib::SharedRef<class UIView> view)
{
	m_views.emplace_back(std::move(view));
}

void UIViewsContainer::RemoveView(UIViewID viewID)
{
	const auto foundView = std::find_if(std::begin(m_views), std::end(m_views),
										[ viewID ](const lib::SharedPtr<UIView>& view)
										{
											return view->GetID() == viewID;
										});

	if (foundView != std::end(m_views))
	{
		m_views.erase(foundView);
	}
}

void UIViewsContainer::RemoveView(const lib::SharedPtr<UIView>& view)
{
	const auto foundView = std::find(std::begin(m_views), std::end(m_views), view);
	if (foundView != std::end(m_views))
	{
		m_views.erase(foundView);
	}
}

void UIViewsContainer::DrawViews(const UIViewDrawParams& params)
{
	SPT_PROFILER_FUNCTION();

	// create copy, in case of adding new windows during draw
	lib::DynamicArray<lib::SharedPtr<UIView>> viewsCopy = std::move(m_views);

	lib::HashSet<lib::SharedPtr<UIView>> closedViews;

	for (const lib::SharedPtr<UIView>& view : viewsCopy)
	{
		const EViewDrawResultActions childResult = view->Draw(params);
		
		if (lib::HasAnyFlag(childResult, EViewDrawResultActions::Close))
		{
			closedViews.emplace(view);
		}
	}

	if(!closedViews.empty())
	{
		lib::RemoveAllBy(viewsCopy,
						 [ &closedViews ](const lib::SharedPtr<UIView>& view)
						 {
							 return closedViews.contains(view);
						 });
	}

	if (m_views.empty())
	{
		m_views = std::move(viewsCopy);
	}
	else
	{
		m_views.reserve(m_views.size() + viewsCopy.size());
		std::move(std::begin(viewsCopy), std::end(viewsCopy), std::back_inserter(m_views));
	}
}

} // spt::scui

