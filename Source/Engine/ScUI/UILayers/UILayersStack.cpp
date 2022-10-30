#include "UILayers/UILayersStack.h"

namespace spt::scui
{

UILayersStack::UILayersStack()
{ }

void UILayersStack::Draw()
{
	SPT_PROFILER_FUNCTION();

	for (const lib::UniquePtr<UILayer>& layer : m_layers)
	{
		layer->Draw();
	}
}

void UILayersStack::Remove(UILayerID id)
{
	SPT_PROFILER_FUNCTION();

	const auto foundLayer = std::find_if(std::cbegin(m_layers), std::cend(m_layers),
										 [id](const lib::UniquePtr<UILayer>& layer)
										 {
											 return layer->GetID() == id;
										 });

	if (foundLayer != std::end(m_layers))
	{
		m_layers.erase(foundLayer);
	}
}

} // spt::scui
