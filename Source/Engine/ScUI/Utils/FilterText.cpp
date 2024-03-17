#include "FilterText.h"
#include "Utility/String/StringUtils.h"


namespace spt::scui
{

FilterText::FilterText(lib::HashedString name)
	: m_name(name)
{
	m_text[0] = '\0';
}

void FilterText::Draw()
{
	ImGui::Columns(2);
	if (ImGui::InputText(m_name.GetData(), m_text.data(), m_text.size()))
	{
		OnTextChanged();
	}
	ImGui::NextColumn();
	if (ImGui::Button("Clear"))
	{
		m_text[0] = '\0';
	}
	ImGui::EndColumns();

	ImGui::Checkbox("Match case", &m_matchCase);
}

void FilterText::OnTextChanged()
{
	m_filters.clear();

	// divide m_text into words
	std::istringstream iss(m_text.data());

	lib::String word;
	while (std::getline(iss, word, ' '))
	{
		lib::String lowerWord = lib::StringUtils::ToLower(word);
		m_filters.emplace_back(Filter{ std::move(word), std::move(lowerWord) });
	}
}

Bool FilterText::IsMatching(lib::StringView text) const
{
	lib::String lowerText;
	if (!m_matchCase)
	{
		lowerText = lib::StringUtils::ToLower(text);
	}
	const lib::StringView filteredText = m_matchCase ? text : lib::StringView(lowerText);
	
	return std::all_of(m_filters.cbegin(), m_filters.cend(),
					   [matchCase = m_matchCase, filteredText](const Filter& filter)
					   {
						   return filteredText.find(matchCase ? filter.text : filter.lowerText) != lib::String::npos;
					   });
}

} // spt::scui
