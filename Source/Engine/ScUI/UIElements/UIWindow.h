#pragma once

#include "SculptorCoreTypes.h"
#include "ScUIMacros.h"
#include "UIContext.h"
#include "UILayers/UILayersStack.h"


namespace spt::scui
{

class SCUI_API UIWindow
{
public:

	UIWindow(const lib::HashedString& name);

	const lib::HashedString& GetName() const;

	void Draw(ui::UIContext context);

	template<typename TLayerType, typename... TArgs>
	UILayerID PushLayer(TArgs&&... args)
	{
		return m_layers.Push<TLayerType>(std::forward<TArgs>(args)...);
	}

	void RemoveLayer(UILayerID layerID)
	{
		m_layers.Remove(layerID);
	}

private:

	lib::HashedString m_name;
	UILayersStack	m_layers;
};

} // spt::scui