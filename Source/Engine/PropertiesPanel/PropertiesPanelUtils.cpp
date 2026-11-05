#include "PropertiesPanelUtils.h"
#include "ImGui/SculptorImGui.h"


namespace spt::ui
{

struct PropertiesDrawersRegistry
{
	lib::HashMap<lib::RuntimeTypeInfo, PropertyDrawer> drawers;
};


PropertiesDrawersRegistry& GetPropertiesDrawersRegistry()
{
	static PropertiesDrawersRegistry registry;
	return registry;
}


void RegisterPropertyDrawer(const lib::RuntimeTypeInfo& type, PropertyDrawer drawer)
{
	GetPropertiesDrawersRegistry().drawers.emplace(type, std::move(drawer));
}


PropertyDrawer GetPropertyDrawer(const lib::RuntimeTypeInfo& type)
{
	auto& drawers = GetPropertiesDrawersRegistry().drawers;
	const auto it = drawers.find(type);
	return it != drawers.end() ? it->second : PropertyDrawer{};
}

void DrawTypeTooltip(const lib::RuntimeTypeInfo& type)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		ImGui::SetTooltip("%.*s", static_cast<Int32>(type.name.size()), type.name.data());
	}
}

void DrawPropertyName(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data)
{
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%.*s", static_cast<Int32>(displayName.size()), displayName.data());
	DrawTypeTooltip(member.type);
	ImGui::TableNextColumn();
}

} // spt::ui
