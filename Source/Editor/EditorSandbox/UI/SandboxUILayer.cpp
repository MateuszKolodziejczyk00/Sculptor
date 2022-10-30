#include "UI/SandboxUILayer.h"
#include "ImGui/SculptorImGui.h"

namespace spt::ed
{

SandboxUILayer::SandboxUILayer(const scui::LayerDefinition& definition, const SandboxRenderer& renderer)
	: Super(definition)
	, m_renderer(&renderer)
{ }

void SandboxUILayer::DrawUI()
{
	Super::DrawUI();

	ImGui::Text("ASDASDASDASDASDASDASD");
}

} // spt::ed
