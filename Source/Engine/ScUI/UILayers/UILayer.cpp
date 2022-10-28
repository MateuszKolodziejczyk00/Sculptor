#include "UILayer.h"

namespace spt::scui
{

UILayer::UILayer(const LayerDefinition& definition)
	: m_name(definition.name)
{ }

void UILayer::SetID(UILayerID id)
{
	m_id = id;
}

UILayerID UILayer::GetID() const
{
	return m_id;
}

void UILayer::Draw(Real32 deltaTime)
{
	SPT_PROFILER_SCOPE((lib::String("Draw") + GetName().ToString()).data());

	DrawUI(deltaTime);
}

const lib::HashedString& UILayer::GetName() const
{
	return m_name;
}

void UILayer::DrawUI(Real32 deltaTime)
{

}

} // spt::scui
