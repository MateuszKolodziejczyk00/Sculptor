#include "UILayers/UILayersStack..h"

namespace spt::scui
{

UILayersStack::UILayersStack()
{ }

void UILayersStack::Draw(Real32 deltaTime)
{
	SPT_PROFILER_FUNCTION();

	for (const lib::UniquePtr<UILayer>& layer : m_layers)
	{
		layer->Draw(deltaTime);
	}
}

} // spt::scui
