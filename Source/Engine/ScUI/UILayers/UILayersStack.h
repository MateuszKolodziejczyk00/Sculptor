#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"
#include "UILayers/UILayer.h"


namespace spt::scui
{

class SCUI_API UILayersStack
{
public:

	UILayersStack();

	UILayersStack(const UILayersStack& rhs) = delete;
	UILayersStack& operator=(const UILayersStack& rhs) = delete;

	template<typename TLayerType, typename... TArgs>
	UILayerID Push(TArgs&&... args)
	{
		SPT_PROFILER_FUNCTION();

		lib::UniquePtr<UILayerID> layerInstance = std::make_unique<TLayerType>(std::forward<TArgs>(args)...);

		UILayerID layerID = UILayerID::GenerateID();
		layerInstance->SetID(layerID);

		m_layers.emplace(std::move(layerInstance));

		return layerID;
	}

	void Remove(UILayerID id);

	void Draw();

private:

	lib::DynamicArray<lib::UniquePtr<UILayer>>	m_layers;
};

} // spt::scui