#pragma once

#include "ScUIMacros.h"
#include "SculptorCoreTypes.h"
#include "UILayers/UIViewTypes.h"


namespace spt::scui
{

class SCUI_API FilterText
{
public:

	explicit FilterText(lib::HashedString name);

	void Draw();

	Bool IsMatching(lib::StringView text) const;

private:

	void OnTextChanged();

	constexpr static size_t s_textSize = 256;

	lib::HashedString m_name;

	lib::StaticArray<char, s_textSize> m_text;

	struct Filter
	{
		lib::String text;
		lib::String lowerText;
	};

	lib::DynamicArray<Filter> m_filters;

	Bool m_matchCase = false;
};

} // spt::scui
