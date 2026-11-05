#include "PropertiesPanel.h"
#include "PropertiesPanelUtils.h"
#include "ImGui/SculptorImGui.h"


namespace spt::ui
{

inline lib::StringView PrettifyName(lib::StringView name, lib::Span<char> buffer)
{
	SPT_CHECK(!name.empty());
	SPT_CHECK(buffer.size() >= name.size() * 2u);

	SizeType length = 0u;
	for (SizeType i = 0u; i < name.size(); ++i)
	{
		const char c    = name[i];
		const char prev = i > 0u ? name[i - 1u] : '\0';
		const char next = i + 1u < name.size() ? name[i + 1u] : '\0';

		const Bool wordStart = i > 0u
							&& ((std::isupper(c) && std::islower(prev))
								|| (std::isupper(c) && std::isupper(prev) && std::islower(next))
								|| (std::isdigit(c) && std::isalpha(prev)));
		if (wordStart)
		{
			buffer[length++] = ' ';
		}

		buffer[length++] = i == 0u ? static_cast<char>(std::toupper(c)) : c;
	}

	return lib::StringView(buffer.data(), length);
}


static Bool GenericDrawProperty(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::Span<Byte> memberData);


static Bool UnknownPropertyDrawer(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data)
{
	DrawPropertyName(context, member, displayName, data);

	const ImVec4 warningColor(0.9f, 0.65f, 0.25f, 1.f);

	const lib::StringView typeName  = member.type.name;
	// Template arguments have their own namespaces, only the ones before the argument list belong to the type
	const lib::StringView shortName = typeName.substr(typeName.find_last_of(':', typeName.find('<')) + 1u);

	if (ImGui::InvisibleButton("##Value", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight())))
	{
		ImGui::SetClipboardText(lib::String(typeName).c_str());
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	ImDrawList* drawList    = ImGui::GetWindowDrawList();
	const ImVec2 frameMin   = ImGui::GetItemRectMin();
	const ImVec2 frameMax   = ImGui::GetItemRectMax();
	const Real32 height     = frameMax.y - frameMin.y;

	const ImGuiCol frameColor = ImGui::IsItemActive() ? ImGuiCol_FrameBgActive : (ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	drawList->AddRectFilled(frameMin, frameMax, ImGui::GetColorU32(frameColor), style.FrameRounding);

	drawList->PushClipRect(frameMin, frameMax, true);
	for (Real32 x = frameMin.x; x < frameMax.x + height; x += ImGui::GetFontSize() * 0.6f)
	{
		drawList->AddLine(ImVec2(x, frameMin.y), ImVec2(x - height, frameMax.y), ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.1f));
	}
	drawList->PopClipRect();

	const Real32 accentWidth = 3.f;
	drawList->AddRectFilled(frameMin, ImVec2(frameMin.x + accentWidth, frameMax.y), ImGui::GetColorU32(warningColor), style.FrameRounding);

	const Real32 textMaxX = frameMax.x - style.FramePadding.x;
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
	ImGui::RenderTextEllipsis(drawList,
							  ImVec2(frameMin.x + accentWidth + style.FramePadding.x, frameMin.y + style.FramePadding.y),
							  ImVec2(textMaxX, frameMax.y),
							  textMaxX,
							  textMaxX,
							  shortName.data(),
							  shortName.data() + shortName.size(),
							  nullptr);
	ImGui::PopStyleColor();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		ImGui::BeginTooltip();
		ImGui::TextColored(warningColor, "No property drawer registered");
		ImGui::TextUnformatted(typeName.data(), typeName.data() + typeName.size());
		ImGui::TextDisabled("Click to copy type name");
		ImGui::EndTooltip();
	}

	return false;
}


// Sign extension keeps values of signed enums and enumerators following negative ones correct regardless of the underlying type size
static Int64 ReadEnumValue(const void* data, Uint32 size)
{
	SPT_CHECK(size > 0u && size <= sizeof(Int64));

	Int64 value = 0;
	std::memcpy(&value, data, size);

	const Uint32 unusedBits = (sizeof(Int64) - size) * 8u;
	return (value << unusedBits) >> unusedBits;
}


template<typename TCallable>
static void ForEachEnumValue(const rtti::RTTIEnum& rttiEnum, TCallable&& callable)
{
	Int64 nextValue = 0;

	for (const rtti::RTTIEnumValue& enumValue : rttiEnum.values)
	{
		SPT_CHECK(!enumValue.value || rttiEnum.size <= sizeof(Int32));
		const Int64 value = enumValue.value ? ReadEnumValue(enumValue.value, rttiEnum.size) : nextValue;

		callable(enumValue, value);

		nextValue = value + 1;
	}
}


static Bool DrawEnumMember(const rtti::RTTIEnum& rttiEnum, lib::Span<Byte> data)
{
	SPT_CHECK(data.size() >= rttiEnum.size);

	const Int64 currentValue = ReadEnumValue(data.data(), rttiEnum.size);

	char previewBuffer[128];
	lib::StringView preview;
	ForEachEnumValue(rttiEnum, [&](const rtti::RTTIEnumValue& enumValue, Int64 value)
					 {
						 if (value == currentValue && preview.empty())
						 {
							 preview = PrettifyName(enumValue.name, previewBuffer);
						 }
					 });

	if (preview.empty())
	{
		std::snprintf(previewBuffer, sizeof(previewBuffer), "%lld", currentValue);
	}
	else
	{
		previewBuffer[preview.size()] = '\0';
	}

	Bool changed = false;

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##Value", previewBuffer))
	{
		ForEachEnumValue(rttiEnum, [&](const rtti::RTTIEnumValue& enumValue, Int64 value)
						 {
							 char nameBuffer[128];
							 nameBuffer[PrettifyName(enumValue.name, nameBuffer).size()] = '\0';

							 const Bool isSelected = value == currentValue;

							 if (ImGui::Selectable(nameBuffer, isSelected))
							 {
								 std::memcpy(data.data(), &value, rttiEnum.size);
								 changed |= !isSelected;
							 }

							 if (isSelected)
							 {
								 ImGui::SetItemDefaultFocus();
							 }
						 });

		ImGui::EndCombo();
	}

	return changed;
}

static Bool EnumPropertyDrawer(const PropertyDrawContext& context, const rtti::RTTIMember& member, const rtti::RTTIEnum& enumType, lib::StringView displayName, lib::Span<Byte> data)
{
	DrawPropertyName(context, member, displayName, data);

	return DrawEnumMember(enumType, data);
}


static Bool StructPropertyDrawer(const PropertyDrawContext& context, const rtti::RTTIMember& member, const rtti::RTTIStruct& structType, lib::StringView displayName, lib::Span<Byte> data, Bool nameMatches)
{
	const ImGuiStyle& style = ImGui::GetStyle();

	ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));

	// Matches the collapse arrow placement of ImGui::TreeNodeBehavior with ImGuiTreeNodeFlags_FramePadding
	const Real32 guideX      = ImGui::GetCursorScreenPos().x + style.FramePadding.x + ImGui::GetFontSize() * 0.5f;
	const Real32 childIndent = ImGui::GetFontSize() + style.FramePadding.x * 2.f;

	if (!nameMatches)
	{
		ImGui::SetNextItemOpen(true);
	}

	const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	const Bool isOpen = ImGui::TreeNodeEx("##Struct", flags, "%.*s", static_cast<Int32>(displayName.size()), displayName.data());
	DrawTypeTooltip(member.type);

	Bool changed = false;

	if (isOpen)
	{
		PropertyDrawContext childContext;
		childContext.filter = nameMatches ? lib::StringView() : context.filter;

		const Real32 guideMinY = ImGui::GetItemRectMax().y;

		ImGui::Indent(childIndent);
		for (const rtti::RTTIMember& childMember : structType.members)
		{
			changed |= GenericDrawProperty(childContext, childMember, data.subspan(childMember.offset));
		}
		ImGui::Unindent(childIndent);

		const Real32 guideMaxY = ImGui::GetItemRectMax().y;
		if (guideMaxY > guideMinY)
		{
			// Children may end in the value column, guide has to be drawn with the name column clip rect
			ImGui::TableSetColumnIndex(0);
			ImGui::GetWindowDrawList()->AddLine(ImVec2(guideX, guideMinY), ImVec2(guideX, guideMaxY), ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.3f));
		}
	}

	return changed;
}


static Bool MatchesFilter(lib::StringView text, lib::StringView filter)
{
	SizeType matchedNum = 0u;
	for (SizeType i = 0u; i < text.size() && matchedNum < filter.size(); ++i)
	{
		if (std::tolower(static_cast<Uint8>(text[i])) == std::tolower(static_cast<Uint8>(filter[matchedNum])))
		{
			++matchedNum;
		}
	}

	return matchedNum == filter.size();
}


static Bool HasFilterMatch(const rtti::RTTIStruct& rttiStruct, lib::StringView filter)
{
	for (const rtti::RTTIMember& member : rttiStruct.members)
	{
		const rtti::RTTIPrimitive* memberPrimitive = rtti::GetType(member.type);
		const rtti::RTTIStruct* memberStruct = memberPrimitive ? memberPrimitive->AsStruct() : nullptr;

		char nameBuffer[128];
		if (MatchesFilter(PrettifyName(member.name, nameBuffer), filter) || (memberStruct && HasFilterMatch(*memberStruct, filter)))
		{
			return true;
		}
	}

	return false;
}


static lib::StringView DrawFilter(lib::Span<char> buffer)
{
	SPT_CHECK(buffer.size() % sizeof(Int32) == 0u);

	ImGuiStorage* storage    = ImGui::GetStateStorage();
	const ImGuiID filterID   = ImGui::GetID("##Filter");
	const Uint32 entriesNum  = static_cast<Uint32>(buffer.size() / sizeof(Int32));

	for (Uint32 i = 0u; i < entriesNum; ++i)
	{
		const Int32 entry = storage->GetInt(ImHashData(&i, sizeof(i), filterID));
		std::memcpy(&buffer[i * sizeof(Int32)], &entry, sizeof(Int32));
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputTextWithHint("##Filter", "Filter", buffer.data(), buffer.size()))
	{
		for (Uint32 i = 0u; i < entriesNum; ++i)
		{
			Int32 entry = 0;
			std::memcpy(&entry, &buffer[i * sizeof(Int32)], sizeof(Int32));
			storage->SetInt(ImHashData(&i, sizeof(i), filterID), entry);
		}
	}

	return lib::StringView(buffer.data());
}


static Bool GenericDrawProperty(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::Span<Byte> memberData)
{
	const rtti::RTTIPrimitive* rttiPrimitive = rtti::GetType(member.type);
	const rtti::RTTIStruct* rttiStruct = rttiPrimitive ? rttiPrimitive->AsStruct() : nullptr;
	const rtti::RTTIEnum* rttiEnum     = rttiPrimitive ? rttiPrimitive->AsEnum()   : nullptr;

	char nameBuffer[128];
	const lib::StringView displayName = PrettifyName(member.name, nameBuffer);

	const Bool nameMatches = MatchesFilter(displayName, context.filter);
	if (!nameMatches && !(rttiStruct && HasFilterMatch(*rttiStruct, context.filter)))
	{
		return false;
	}

	ImGui::PushID(member.name.data(), member.name.data() + member.name.size());
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	Bool changed = false;

	const PropertyDrawer drawer = GetPropertyDrawer(member.type);
	if (drawer.IsValid())
	{
		changed = drawer(context, member, displayName, memberData);
	}
	else if (rttiStruct)
	{
		changed = StructPropertyDrawer(context, member, *rttiStruct, displayName, memberData, nameMatches);
	}
	else if (rttiEnum)
	{
		changed = EnumPropertyDrawer(context, member, *rttiEnum, displayName, memberData);
	}
	else
	{
		changed = UnknownPropertyDrawer(context, member, displayName, memberData);
	}

	ImGui::PopID();

	return changed;
}


template<typename TDrawRows>
static Bool DrawPanel(lib::StringView name, lib::RuntimeTypeInfo type, TDrawRows&& drawRows)
{
	SPT_CHECK(!name.empty());

	Bool changed = false;

	ImGui::PushID(name.data(), name.data() + name.size());

	ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	const Bool isOpen = ImGui::TreeNodeEx("##Root", headerFlags, "%.*s", static_cast<Int32>(name.size()), name.data());
	ImGui::PopStyleColor(3);
	if (type.IsValid())
	{
		DrawTypeTooltip(type);
	}

	const ImVec2 headerMin   = ImGui::GetItemRectMin();
	const ImVec2 headerMax   = ImGui::GetItemRectMax();
	const Real32 accentInset = ImGui::GetStyle().FramePadding.y;
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(headerMin.x, headerMin.y + accentInset),
											  ImVec2(headerMin.x + 3.f, headerMax.y - accentInset),
											  ImGui::GetColorU32(ImGuiCol_CheckMark),
											  1.5f);

	if (isOpen)
	{
		char filterBuffer[64];
		const lib::StringView filter = DrawFilter(filterBuffer);

		if (ImGui::BeginTable("##Properties", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);

			changed = drawRows(filter);

			ImGui::EndTable();
		}
	}

	ImGui::PopID();

	return changed;
}

Bool DrawPropertiesPanel(const lib::RuntimeTypeInfo& type, lib::Span<Byte> data)
{
	const lib::StringView typeName = type.name.substr(type.name.find_last_of(": ") + 1u);

	char nameBuffer[128];
	return DrawPropertiesPanel(PrettifyName(typeName, nameBuffer), type, data);
}


Bool DrawPropertiesPanel(lib::StringView name, const lib::RuntimeTypeInfo& type, lib::Span<Byte> data)
{
	const rtti::RTTIPrimitive* rttiPrimitive = rtti::GetType(type);
	SPT_CHECK_MSG(!!rttiPrimitive, "Type {} is not registered in RTTI", type.name);

	const rtti::RTTIStruct* rttiStruct = rttiPrimitive->AsStruct();
	SPT_CHECK_MSG(!!rttiStruct, "Type {} is not a struct", type.name);

	return DrawPanel(name, type, [&](lib::StringView filter) { return GenericDrawProperty(PropertyDrawContext{ filter }, rtti::RTTIMember{ name, type }, data); });
}


Bool DrawPropertiesPanel(lib::StringView name, lib::Span<const Property> properties)
{
	return DrawPanel(name, lib::RuntimeTypeInfo{}, [&](lib::StringView filter)
					 {
						 Bool changed = false;
						 for (const Property& property : properties)
						 {
							 changed |= GenericDrawProperty(PropertyDrawContext{ filter }, rtti::RTTIMember{ property.name, property.type }, property.data);
						 }
						 return changed;
					 });
}

} // spt::ui
