#include "FilterText.h"
#include "Utility/String/StringUtils.h"
#include "UIElements/ApplicationUI.h"
#include "InputManager.h"
#include "UIUtils.h"


namespace spt::scui
{

FilterText::FilterText(lib::HashedString name)
	: m_name(name)
{
	m_text[0] = '\0';
}

void FilterText::Draw(const char* hint)
{
	const lib::String inputLabel = "##" + m_name.ToString();
	const ImGuiID id = ImGui::GetID(inputLabel.c_str());
	ui::UIContext uiContext = scui::ApplicationUI::GetCurrentContext().GetUIContext();
	ui::UIUtils::SetShortcut(uiContext, id, ui::ShortcutBinding::Create(inp::EKey::LCtrl, inp::EKey::F));

	const ImGuiStyle& style = ImGui::GetStyle();
	const Real32 matchCaseWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Aa").x;
	const Real32 clearWidth     = ImGui::CalcTextSize("Clear").x + style.FramePadding.x * 2.f;

	ImGui::SetNextItemWidth(-(matchCaseWidth + clearWidth + style.ItemSpacing.x * 2.f));
	if (ImGui::InputTextWithHint(inputLabel.c_str(), hint, m_text.data(), m_text.size()))
	{
		OnTextChanged();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Aa", &m_matchCase);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Match case");
	}

	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		m_text[0] = '\0';
		OnTextChanged();
	}
}

void FilterText::OnTextChanged()
{
	m_filters.clear();

	// divide m_text into words
	std::istringstream iss(m_text.data());

	lib::String element;
	while (std::getline(iss, element, '|'))
	{
		m_filters.emplace_back();

		std::istringstream elementISS(element.data());

		lib::String word;
		while (std::getline(elementISS, word, ' '))
		{
			lib::String lowerWord = lib::StringUtils::ToLower(word);
			m_filters.back().emplace_back(Filter{ std::move(word), std::move(lowerWord) });
		}
	}
}

Bool FilterText::IsMatching(lib::StringView text, Uint32 elementIdx /* = 0u */) const
{
	if (elementIdx >= m_filters.size())
	{
		return true;
	}

	lib::String lowerText;
	if (!m_matchCase)
	{
		lowerText = lib::StringUtils::ToLower(text);
	}
	const lib::StringView filteredText = m_matchCase ? text : lib::StringView(lowerText);
	
	return std::all_of(m_filters[elementIdx].cbegin(), m_filters[elementIdx].cend(),
					   [matchCase = m_matchCase, filteredText](const Filter& filter)
					   {
						   return filteredText.find(matchCase ? filter.text : filter.lowerText) != lib::String::npos;
					   });
}

} // spt::scui
