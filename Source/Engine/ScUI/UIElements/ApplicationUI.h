#pragma once

#include "SculptorCoreTypes.h"
#include "ScUIMacros.h"
#include "UIContext.h"
#include "UILayers/UIView.h"


namespace spt::scui
{

class SCUI_API ApplicationUI
{
public:

	static ApplicationUI& GetInstance();

	template<typename TViewType, typename... TArgs>
	static lib::SharedRef<TViewType> OpenView(TArgs&&... args)
	{
		const lib::SharedRef<TViewType> view = lib::MakeShared<TViewType>(std::forward<TArgs>(args)...);
		AddView(std::move(view));
		return view;
	}

	static void AddView(lib::SharedRef<UIView> view);
	
	static void CloseView(const lib::SharedPtr<UIView>& view);
	static void CloseView(UIViewID id);

	static void Draw(ui::UIContext context);

private:

	ApplicationUI() = default;

	UIViewsContainer m_views;
};

} // spt::scui