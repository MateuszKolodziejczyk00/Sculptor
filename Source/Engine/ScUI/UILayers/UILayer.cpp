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

void UILayer::Draw()
{
	SPT_PROFILER_SCOPE(GetName().GetData());

	DrawUI();
}

const lib::HashedString& UILayer::GetName() const
{
	return m_name;
}

void UILayer::DrawUI()
{

}

} // spt::scui
