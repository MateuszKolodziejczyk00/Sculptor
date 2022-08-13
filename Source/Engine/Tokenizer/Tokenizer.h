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


using TokensQueue = lib::DynamicArray<TokenInfo>;


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

	TokensQueue							BuildTokens() const;

	lib::DynamicArray<lib::StringView>	DivideByTokens(const TokensQueue& tokens, Flags32 flags) const;

	lib::StringView						GetBetweenTokens(const TokenInfo& first, const TokenInfo& second) const;

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
TokensQueue Tokenizer<TTokenizerDictionary>::BuildTokens() const
{
	SPT_PROFILE_FUNCTION();

	TokensQueue tokens;

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
			const auto emplaceLocation = std::lower_bound(std::begin(tokens), std::end(tokens), token);

			tokens.emplace(emplaceLocation, token);
		}
	}

	return tokens;
}

template<typename TTokenizerDictionary>
lib::DynamicArray<lib::StringView> Tokenizer<TTokenizerDictionary>::DivideByTokens(const TokensQueue& tokens, Flags32 flags) const
{
	lib::DynamicArray<lib::StringView> strings;
	strings.reserve(tokens.size() + 1);

	if (tokens.empty())
	{
		strings.emplace_back(m_string);
		return strings;
	}

	const Bool includeStringBeforeFirstToken = tokens[0].m_tokenPosition != 0 && (flags & EDivideByTokensFlags::IncludeStringBeforeFirstToken);

	if (includeStringBeforeFirstToken)
	{
		strings.emplace_back(lib::StringView(std::begin(m_string), std::begin(m_string) + tokens[0].m_tokenPosition));
	}

	for (SizeType idx = 1; idx < tokens.size(); ++idx)
	{
		strings.emplace_back(GetBetweenTokens(tokens[idx - 1], tokens[idx]));
	}

	if ((flags & EDivideByTokensFlags::IncludeStringAfterLastToken) != 0)
	{
		const TokenInfo lastToken = tokens[tokens.size() - 1];
		const SizeType lastTokenEndPosition = GetTokenEndPosition(lastToken);

		if(lastTokenEndPosition < m_string.size())
		{
			strings.emplace_back(lib::StringView(std::begin(m_string) + lastTokenEndPosition, std::begin(m_string) + m_string.size()));
		}
	}

	return strings;
}

template<typename TTokenizerDictionary>
lib::StringView Tokenizer<TTokenizerDictionary>::GetBetweenTokens(const TokenInfo& first, const TokenInfo& second) const
{
	SPT_CHECK(first.IsValid() && second.IsValid() && first < second);

	const SizeType firstTokenLength = GetTokenLength(first.m_tokenTypeIdx);

	const SizeType beginPosition	= first.m_tokenPosition + firstTokenLength;
	const SizeType endPosition		= second.m_tokenPosition;

	return lib::StringView(std::begin(m_string) + beginPosition, std::begin(m_string) + endPosition);
}

}
