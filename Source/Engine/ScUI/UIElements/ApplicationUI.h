#pragma once

#include "SculptorCoreTypes.h"
#include "ScUIMacros.h"
#include "UIContext.h"
#include "UIElements/UIWindow.h"


namespace spt::scui
{

class SCUI_API ApplicationUI
{
public:

	static ApplicationUI& GetInstance();

	template<typename TLayerType, typename... TArgs>
	static lib::SharedRef<UIWindow> OpenWindowWithLayer(const lib::HashedString& name, TArgs&&... args)
	{
		lib::SharedRef<UIWindow> window = OpenWindow(name);
		window->PushLayer<TLayerType>(std::forward<TArgs>(args)...);
		return window;
	}

	static lib::SharedRef<UIWindow> OpenWindow(const lib::HashedString& name);
	static void CloseWindow(const lib::HashedString& name);

	static void Draw(ui::UIContext context);

private:

	ApplicationUI() = default;

	lib::DynamicArray<lib::SharedPtr<UIWindow>> m_windows;
};

} // spt::scui