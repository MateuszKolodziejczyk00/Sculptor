#pragma once

#include "SculptorCoreTypes.h"

namespace spt::tkn
{

using Token		= lib::String;
using TokenView = lib::String;


template<SizeType dictionarySize> 
using Dictionary = lib::StaticArray<Token, dictionarySize>;


struct TokenInfo
{
	TokenInfo()
		: m_tokenTypeIdx(idxNone<SizeType>)
		, m_tokenPosition(idxNone<SizeType>)
	{ }

	TokenInfo(SizeType tokenTypeIdx, SizeType tokenPosition)
		: m_tokenTypeIdx(tokenTypeIdx)
		, m_tokenPosition(tokenPosition)
	{ }

	Bool IsValid() const
	{
		return m_tokenTypeIdx != idxNone<SizeType> && m_tokenPosition != idxNone<SizeType>;
	}

	Bool operator<(TokenInfo rhs) const
	{
		return m_tokenPosition < rhs.m_tokenPosition;
	}
	
	Bool operator>(TokenInfo rhs) const
	{
		return m_tokenPosition > rhs.m_tokenPosition;
	}

	SizeType			m_tokenTypeIdx;
	SizeType			m_tokenPosition;
};


using TokensArray = lib::DynamicArray<TokenInfo>;


namespace EDivideByTokensFlags
{

enum Flags : Flags32
{
	None							= 0,
	IncludeStringBeforeFirstToken	= BIT(1),
	IncludeStringAfterLastToken		= BIT(2) 
};

}


template<typename TTokenizerDictionary>
class Tokenizer
{
public:

	Tokenizer(lib::StringView string, const TTokenizerDictionary& dictionary);

	TokensArray							BuildTokensArray() const;

	lib::DynamicArray<lib::StringView>	DivideByTokens(const TokensArray& tokens, Flags32 flags) const;

	lib::StringView						GetStringBetweenTokens(const TokenInfo& first, const TokenInfo& second) const;

	lib::StringView						GetStringBeforeToken(const TokenInfo& token) const;
	lib::StringView						GetStringAfterToken(const TokenInfo& token) const;

private:

	SizeType							GetTokenLength(SizeType tokenTypeIdx) const
	{
		return m_dictionary[tokenTypeIdx].length();
	}

	SizeType							GetTokenEndPosition(TokenInfo token) const
	{
		return token.m_tokenPosition + GetTokenLength(token.m_tokenTypeIdx):
	}

	lib::StringView						m_string;
	const TTokenizerDictionary&			m_dictionary;
};


template<typename TTokenizerDictionary>
Tokenizer<TTokenizerDictionary>::Tokenizer(lib::StringView string, const TTokenizerDictionary& dictionary)
	: m_string(string)
	, m_dictionary(dictionary)
{ }

template<typename TTokenizerDictionary>
TokensArray Tokenizer<TTokenizerDictionary>::BuildTokensArray() const
{
	SPT_PROFILE_FUNCTION();

	TokensArray tokens;

	// It's kinda slow implementation but hopefully for now it will be enough
	for (SizeType tokenTypeIdx = 0; tokenTypeIdx < m_dictionary.size(); ++tokenTypeIdx)
	{
		const Token& token 

		SizeType currentPosition = 0;

		Bool bContinue = true;
		while (true)
		{
			currentPosition = m_string.find(token, currentPosition);

			if (currentPosition == lib::String::npos)
			{
				bContinue = false;
				break;
			}

			const TokenInfo token(tokenTypeIdx, currentPosition);
			const auto emplaceLocation = std::lower_bound(std::cbegin(tokens), std::cend(tokens), token);

			tokens.emplace(emplaceLocation, token);
		}
	}

	return tokens;
}

template<typename TTokenizerDictionary>
lib::DynamicArray<lib::StringView> Tokenizer<TTokenizerDictionary>::DivideByTokens(const TokensArray& tokens, Flags32 flags) const
{
	lib::DynamicArray<lib::StringView> strings;
	strings.reserve(tokens.size() + 1);

	if (tokens.empty())
	{
		strings.emplace_back(m_string);
		return strings;
	}

	const Bool includeStringBeforeFirstToken = tokens[0].m_tokenPosition != 0 && ;

	if ((flags & EDivideByTokensFlags::IncludeStringBeforeFirstToken) != 0)
	{
		const lib::StringView stringBeforeToken = GetStringAfterToken(tokens[0]);

		if(!stringBeforeToken.empty())
		{
			strings.emplace_back(stringBeforeToken);
		}
	}

	for (SizeType idx = 1; idx < tokens.size(); ++idx)
	{
		strings.emplace_back(GetStringBetweenTokens(tokens[idx - 1], tokens[idx]));
	}

	if ((flags & EDivideByTokensFlags::IncludeStringAfterLastToken) != 0)
	{
		const lib::StringView stringAfterToken = GetStringAfterToken(tokens[tokens.size() - 1]);

		if(!stringAfterToken.empty())
		{
			strings.emplace_back(stringAfterToken);
		}
	}

	return strings;
}

template<typename TTokenizerDictionary>
lib::StringView Tokenizer<TTokenizerDictionary>::GetStringBetweenTokens(const TokenInfo& first, const TokenInfo& second) const
{
	SPT_CHECK(first.IsValid() && second.IsValid() && first < second);

	const SizeType firstTokenLength = GetTokenLength(first.m_tokenTypeIdx);

	const SizeType beginPosition	= first.m_tokenPosition + firstTokenLength;
	const SizeType endPosition		= second.m_tokenPosition;

	return lib::StringView(std::cbegin(m_string) + beginPosition, std::cbegin(m_string) + endPosition);
}

template<typename TTokenizerDictionary>
lib::StringView Tokenizer<TTokenizerDictionary>::GetStringBeforeToken(const TokenInfo& token) const
{
	SPT_CHECK(token.IsValid());

	return lib::StringView(std::cbegin(m_string), std::cbegin(m_string) + token.m_tokenPosition);
}

template<typename TTokenizerDictionary>
lib::StringView Tokenizer<TTokenizerDictionary>::GetStringAfterToken(const TokenInfo& token) const
{
	SPT_CHECK(token.IsValid());

	const SizeType tokenEndPosition = GetTokenEndPosition(token);

	return lib::StringView(std::cbegin(m_string) + tokenEndPosition, std::cend(m_string));
}

}
