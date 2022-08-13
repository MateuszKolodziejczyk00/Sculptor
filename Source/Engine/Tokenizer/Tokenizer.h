#pragma once

#include "SculptorCoreTypes.h"
#include "Delegates/Delegate.h"

namespace spt::tkn
{

class TokensProcessor;


using Token		= lib::String;
using TokenView = lib::String;


template<SizeType dictionarySize> 
using Dictionary = lib::StaticArray<Token, dictionarySize>;


struct TokenInfo
{
	TokenInfo()
		: m_tokenTypeIdx(idxNone<SizeType>)
		, m_tokenPosition(idxNone<SizeType>)
		, m_idx(idxNone<SizeType>)
	{ }

	TokenInfo(SizeType tokenTypeIdx, SizeType tokenPosition)
		: m_tokenTypeIdx(tokenTypeIdx)
		, m_tokenPosition(tokenPosition)
		, m_idx(idxNone<SizeType>)
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
	SizeType			m_idx;
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
		const Token& token = m_dictionary[tokenTypeIdx];

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

			const TokenInfo tokenInfo(tokenTypeIdx, currentPosition);
			const auto emplaceLocation = std::lower_bound(std::cbegin(tokens), std::cend(tokens), tokenInfo);

			tokens.emplace(emplaceLocation, tokenInfo);
		}
	}

	auto idxBuilder = [currentIdx = 0](TokenInfo& token) mutable
	{
		token.m_idx = currentIdx++;
	};

	std::for_each(std::begin(tokens), std::end(tokens), idxBuilder);

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


class TokensVisitor
{
	friend TokensProcessor;

public:

	using TokenVisitorDelegate = lib::Delegate<TokenInfo/* tokenInfo*/, const TokensProcessor& /*processor*/>;

	TokenVisitorDelegate& BindToToken(SizeType tokenTypeIdx)
	{
		return m_visitors[tokenTypeIdx];
	}

private:

	void ExecuteOnToken(const TokenInfo& token, const TokensProcessor& processor) const
	{
		const auto foundVisitor = m_visitors.find(token.m_tokenTypeIdx);
		if (foundVisitor != std::cend(m_visitors))
		{
			foundVisitor->second.ExecuteIfBound(token, processor);
		}
	}

	lib::HashMap<SizeType, TokenVisitorDelegate> m_visitors;
};


class TokensProcessor
{
public:

	TokensProcessor(const TokensArray& tokens)
		: m_tokens(tokens)
	{ }

	const TokensArray&		GetTokens() const;

	TokenInfo				FindNextToken(const TokenInfo& currentToken) const;
	TokenInfo				FindNextTokenOfType(const TokenInfo& currentToken, SizeType tokenTypeIdx) const;

	void					VisitAllTokens(const TokensVisitor& visitor) const;

	void					VisitTokens(const TokensVisitor& visitor, const TokenInfo& begin, const TokenInfo end) const;

private:

	void					VisitImpl(const TokensVisitor& visitor, const SizeType begin, const SizeType end) const;

	const TokensArray&		m_tokens;
};

const TokensArray& TokensProcessor::GetTokens() const
{
	return m_tokens;
}

TokenInfo TokensProcessor::FindNextToken(const TokenInfo& currentToken) const
{
	const bool nextTokenExists = currentToken.IsValid() && currentToken.m_idx < m_tokens.size() - 1;
	return nextTokenExists ? m_tokens[currentToken.m_idx + 1] : TokenInfo();
}

TokenInfo TokensProcessor::FindNextTokenOfType(const TokenInfo& currentToken, SizeType tokenTypeIdx) const
{
	if (currentToken.IsValid() && currentToken.m_idx != m_tokens.size() - 1)
	{
		const auto foundToken = std::find_if(std::cbegin(m_tokens) + currentToken.m_idx + 1, std::cend(m_tokens),
			[tokenTypeIdx](const TokenInfo& token)
			{
				return token.m_tokenTypeIdx == tokenTypeIdx;
			});

		return foundToken != std::cend(m_tokens) ? *foundToken : TokenInfo();
	}

	return TokenInfo();
}

void TokensProcessor::VisitAllTokens(const TokensVisitor& visitor) const
{
	VisitImpl(visitor, 0, m_tokens.size());
}

void TokensProcessor::VisitTokens(const TokensVisitor& visitor, const TokenInfo& begin, const TokenInfo end) const
{
	const SizeType beginIdx = begin.IsValid() ? begin.m_idx : 0;
	const SizeType endIdx = end.IsValid() ? end.m_idx : m_tokens.size();

	VisitImpl(visitor, beginIdx, endIdx);
}

void TokensProcessor::VisitImpl(const TokensVisitor& visitor, const SizeType begin, const SizeType end) const
{
	std::for_each(std::cbegin(m_tokens) + begin, std::cend(m_tokens) + end,
		[&visitor, this](const TokenInfo& token)
		{
			visitor.ExecuteOnToken(token, *this);
		});
}

}
