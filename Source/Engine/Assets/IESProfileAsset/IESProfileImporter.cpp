#include "IESProfileImporter.h"


namespace spt::as
{

static lib::StringView Trim(lib::StringView str)
{
	const char* begin = str.data();
	const char* end = begin + str.size();

	while (begin < end && std::isspace(static_cast<unsigned char>(*begin)))
	{
		++begin;
	}

	while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
	{
		--end;
	}

	return lib::StringView(begin, end);
}

static Bool SplitBy(lib::StringView str, const char split, lib::StringView& left, lib::StringView& right)
{
	const auto pos = str.find(split);
	if (pos == lib::StringView::npos)
	{
		left  = lib::StringView();
		right = lib::StringView();
		return false;
	}

	left  = lib::StringView(str.data(), pos);
	right = lib::StringView(str.data() + pos + 1, str.size() - pos - 1);

	return true;
}

static lib::DynamicArray<lib::StringView> TokenizeLine(lib::StringView line)
{
	lib::DynamicArray<lib::StringView> tokens;

	if (line.empty())
	{
		return tokens;
	}

	lib::StringView currentToken;
	lib::StringView remaining = line;
	lib::StringView newRemaining;
	while (SplitBy(remaining, ' ', OUT currentToken, OUT newRemaining))
	{
		const lib::StringView trimmedToken = Trim(currentToken);
		if (!trimmedToken.empty())
		{
			tokens.push_back(trimmedToken);
		}

		remaining = newRemaining;
	}

	if (!remaining.empty())
	{
		tokens.emplace_back(Trim(remaining));
	}

	return tokens;
}


IESProfileDefinition ImportIESProfileFromString(lib::StringView iesData)
{
	std::locale::global(std::locale::classic());

	IESProfileDefinition profile;

	lib::StringView remaining = iesData;

	lib::StringView currentLine;
	while (SplitBy(remaining, '\n', OUT currentLine, OUT remaining))
	{
		if (!remaining.empty())
		{
			if (currentLine.starts_with("TILT="))
			{
				const lib::StringView tiltValue = lib::StringView(currentLine.cbegin() + 5u, currentLine.cend());

				SPT_CHECK_MSG(Trim(tiltValue) == "NONE", "TILT different than NONE is not supported");
				break;
			}
		}
	}

	SplitBy(remaining, '\n', OUT currentLine, OUT remaining);

	lib::DynamicArray<lib::StringView> props = TokenizeLine(Trim(currentLine));

	profile.numLamps = static_cast<Uint32>(std::strtoul(props[0].data(), nullptr, 10));

	profile.lumensPerLamp     = static_cast<Real32>(std::strtof(props[1].data(), nullptr));
	profile.candelaMultiplier = static_cast<Real32>(std::strtof(props[2].data(), nullptr));

	profile.verticalAnglesNum   = static_cast<Uint32>(std::strtoul(props[3].data(), nullptr, 10));
	profile.horizontalAnglesNum = static_cast<Uint32>(std::strtoul(props[4].data(), nullptr, 10));

	profile.photometricType = static_cast<PhotometricType>(std::strtoul(props[5].data(), nullptr, 10));

	SPT_CHECK(profile.photometricType == PhotometricType::TypeC);

	SplitBy(remaining, '\n', currentLine, remaining);

	profile.verticalAngles.reserve(profile.verticalAnglesNum);
	profile.horizontalAngles.reserve(profile.horizontalAnglesNum);

	while (profile.verticalAngles.size() < profile.verticalAnglesNum)
	{
		SplitBy(remaining, '\n', OUT currentLine, OUT remaining);
		lib::DynamicArray<lib::StringView> angleTokens = TokenizeLine(Trim(currentLine));
		for (const lib::StringView& angleToken : angleTokens)
		{
			const Real32 angle = static_cast<Real32>(std::strtof(angleToken.data(), nullptr));
			profile.verticalAngles.emplace_back(angle);
		}
	}

	while (profile.horizontalAngles.size() < profile.horizontalAnglesNum)
	{
		SplitBy(remaining, '\n', OUT currentLine, OUT remaining);
		lib::DynamicArray<lib::StringView> angleTokens = TokenizeLine(Trim(currentLine));
		for (const lib::StringView& angleToken : angleTokens)
		{
			const Real32 angle = static_cast<Real32>(std::strtof(angleToken.data(), nullptr));
			profile.horizontalAngles.emplace_back(angle);
		}
	}

	const Uint32 candelaValuesNum = profile.verticalAnglesNum * profile.horizontalAnglesNum;
	profile.candela.reserve(candelaValuesNum);

	while (profile.candela.size() < candelaValuesNum)
	{
		SplitBy(remaining, '\n', OUT currentLine, OUT remaining);
		lib::DynamicArray<lib::StringView> candelaTokens = TokenizeLine(Trim(currentLine));
		for (const lib::StringView& candelaToken : candelaTokens)
		{
			const Real32 candelaValue = static_cast<Real32>(std::strtof(candelaToken.data(), nullptr));
			profile.candela.emplace_back(candelaValue);
		}
	}

	return profile;
}

} // spt::as
