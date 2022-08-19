#pragma once

#include "SculptorCoreTypes.h"
#include "Tokenizer.h"


namespace spt::tkn
{

class ArgumentsTokenizer
{
public:

	static lib::DynamicArray<lib::StringView> ExtractArguments(lib::StringView string)
	{
		const auto removeWhiteChars = [](lib::StringView& argument)
		{
			const SizeType properBeginPosition	= argument.find_first_not_of("(),\t ");
			const SizeType properEndPosition	= argument.find_last_not_of("(),\t ");
			SPT_CHECK(properBeginPosition < properEndPosition);

			const SizeType properArgumentLength = properEndPosition - properBeginPosition;
			const lib::StringView properArgument(argument.data() + properBeginPosition, properArgumentLength);
			argument = properArgument;
		};

		[[maybe_unused]]
		constexpr SizeType commaTokenIdx = 0;
		const lib::StaticArray<Token, 1> tokens{ "," };

		const Tokenizer tokenizer(string, tokens);
		const TokensArray tokensArray = tokenizer.BuildTokensArray();

		lib::DynamicArray<lib::StringView> arguments;

		if (!tokensArray.empty())
		{
			const EDivideByTokensFlags divideFlags = lib::Flags(EDivideByTokensFlags::IncludeStringBeforeFirstToken, EDivideByTokensFlags::IncludeStringAfterLastToken);
			arguments = tokenizer.DivideByTokens(tokensArray, divideFlags);

			// At this point arguments contains commas and whitespaces, so remove them
			std::for_each(std::begin(arguments), std::end(arguments), removeWhiteChars);
		}
		else
		{
			// we don't have commas, so we have only one argument
			lib::StringView argument = string;
			removeWhiteChars(argument);
			arguments.push_back(argument);
		}

		return arguments;
	}
};

} // spt::tkn