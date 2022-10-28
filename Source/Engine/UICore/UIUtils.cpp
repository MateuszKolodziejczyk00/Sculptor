#include "UIUtils.h"
#include "ImGui/SculptorImGui.h"

namespace spt::ui
{

math::Vector2f UIUtils::GetWindowContentSize()
{
	const float width	= ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
	const float height	= ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
	return math::Vector2f(width, height);
}

} // spt::ui