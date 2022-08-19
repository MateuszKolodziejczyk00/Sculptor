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


enum class EDivideByTokensFlags : Flags32
{
	None							= 0,
	IncludeStringBeforeFirstToken	= BIT(1),
	IncludeStringAfterLastToken		= BIT(2)
};


template<typename TTokenizerDictionary>
class Tokenizer
{
public:

	Tokenizer(lib::StringView string, const TTokenizerDictionary& dictionary);

	TokensArray							BuildTokensArray() const;

	lib::DynamicArray<lib::StringView>	DivideByTokens(const TokensArray& tokens, EDivideByTokensFlags flags) const;

	lib::StringView						GetStringBetweenTokens(const TokenInfo& first, const TokenInfo& second) const;

	lib::StringView						GetStringBeforeToken(const TokenInfo& token) const;
	lib::StringView						GetStringAfterToken(const TokenInfo& token) const;

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
inline Tokenizer<TTokenizerDictionary>::Tokenizer(lib::StringView string, const TTokenizerDictionary& dictionary)
	: m_string(string)
	, m_dictionary(dictionary)
{ }

template<typename TTokenizerDictionary>
inline TokensArray Tokenizer<TTokenizerDictionary>::BuildTokensArray() const
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
inline lib::DynamicArray<lib::StringView> Tokenizer<TTokenizerDictionary>::DivideByTokens(const TokensArray& tokens, EDivideByTokensFlags flags) const
{
	lib::DynamicArray<lib::StringView> strings;
	strings.reserve(tokens.size() + 1);

	if (tokens.empty())
	{
		strings.emplace_back(m_string);
		return strings;
	}

	if (lib::HasAnyFlag(flags, EDivideByTokensFlags::IncludeStringBeforeFirstToken))
	{
		const lib::StringView stringBeforeToken = GetStringBeforeToken(tokens[0]);

		if(!stringBeforeToken.empty())
		{
			strings.emplace_back(stringBeforeToken);
		}
	}

	for (SizeType idx = 1; idx < tokens.size(); ++idx)
	{
		strings.emplace_back(GetStringBetweenTokens(tokens[idx - 1], tokens[idx]));
	}

	if (lib::HasAnyFlag(flags, EDivideByTokensFlags::IncludeStringAfterLastToken))
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
inline lib::StringView Tokenizer<TTokenizerDictionary>::GetStringBetweenTokens(const TokenInfo& first, const TokenInfo& second) const
{
	SPT_CHECK(first < second);

	const SizeType beginPosition	= first.IsValid() ? first.m_tokenPosition : 0;
	const SizeType endPosition		= second.IsValid() ? second.m_tokenPosition : m_string.size();

	const SizeType length = endPosition - beginPosition;

	return lib::StringView(m_string.data() + beginPosition, length);
}

template<typename TTokenizerDictionary>
inline lib::StringView Tokenizer<TTokenizerDictionary>::GetStringBeforeToken(const TokenInfo& token) const
{
	SPT_CHECK(token.IsValid());

	return lib::StringView(m_string.data(), token.m_tokenPosition);
}

template<typename TTokenizerDictionary>
inline lib::StringView Tokenizer<TTokenizerDictionary>::GetStringAfterToken(const TokenInfo& token) const
{
	SPT_CHECK(token.IsValid());

	const SizeType length = m_string.size() - token.m_tokenPosition;

	return lib::StringView(m_string.data() + token.m_tokenPosition, length);
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

inline const TokensArray& TokensProcessor::GetTokens() const
{
	return m_tokens;
}

inline TokenInfo TokensProcessor::FindNextToken(const TokenInfo& currentToken) const
{
	const bool nextTokenExists = currentToken.IsValid() && currentToken.m_idx < m_tokens.size() - 1;
	return nextTokenExists ? m_tokens[currentToken.m_idx + 1] : TokenInfo();
}

inline TokenInfo TokensProcessor::FindNextTokenOfType(const TokenInfo& currentToken, SizeType tokenTypeIdx) const
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

inline void TokensProcessor::VisitAllTokens(const TokensVisitor& visitor) const
{
	VisitImpl(visitor, 0, m_tokens.size());
}

inline void TokensProcessor::VisitTokens(const TokensVisitor& visitor, const TokenInfo& begin, const TokenInfo end) const
{
	const SizeType beginIdx = begin.IsValid() ? begin.m_idx : 0;
	const SizeType endIdx = end.IsValid() ? end.m_idx : m_tokens.size();

	VisitImpl(visitor, beginIdx, endIdx);
}

inline void TokensProcessor::VisitImpl(const TokensVisitor& visitor, const SizeType begin, const SizeType end) const
{
	std::for_each(std::cbegin(m_tokens) + begin, std::cend(m_tokens) + end,
		[&visitor, this](const TokenInfo& token)
		{
			visitor.ExecuteOnToken(token, *this);
		});
}


class TokenizerUtils
{
public:

	static lib::StringView		GetNearestStringBetween(lib::StringView string, char startCharacter, char endCharacter)
	{
		const SizeType startPosition	= string.find(startCharacter, 0);
		const SizeType endPosition		= string.find(endCharacter, startPosition + 1);

		return startPosition != lib::StringView::npos && endPosition != lib::StringView::npos
			? lib::StringView(string.data() + startPosition + 1, endPosition - startPosition)
			: lib::StringView();
	}

	static lib::StringView		GetStringInNearestBracket(lib::StringView string, SizeType offset)
	{
		SPT_CHECK(offset < string.size());
		const SizeType substringLength = string.size() - offset;
		const lib::StringView substring(string.data() + offset, substringLength);

		return GetNearestStringBetween(string, '(', ')');
	}
};

}
