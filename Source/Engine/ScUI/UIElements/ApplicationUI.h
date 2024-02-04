#pragma once

#include "SculptorCoreTypes.h"
#include "ScUIMacros.h"
#include "UIContext.h"
#include "UILayers/UIView.h"
#include "ScUITypes.h"


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

	static void CloseAllViews();

	static void Draw(const Context& context);

	// Can be called only during Draw()
	static const Context& GetCurrentContext();

private:

	ApplicationUI() = default;

	UIViewsContainer m_views;

	const Context* m_currentContext = nullptr;
};

} // spt::scui