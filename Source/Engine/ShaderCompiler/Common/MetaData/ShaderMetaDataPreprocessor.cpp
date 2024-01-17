#include "ShaderMetaDataPreprocessor.h"
#include "Common/ShaderCompilationInput.h"
#include "Tokenizer.h"
#include "ArgumentsTokenizer.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefsRegistry.h"
#include "Common/ShaderStructs/ShaderStructsRegistry.h"

#include <regex>

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Token =========================================================================================

namespace EMetaDataProcessorToken
{

enum Type : Uint32
{
	BeginMetaData,

	NUM
};

} // EMetaDataProcessorToken

static const lib::StaticArray<tkn::Token, EMetaDataProcessorToken::NUM> g_metaDataProcessorTokens
{
	tkn::Token("META(")
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace helper
{

static lib::HashedString ExtractParameterName(tkn::TokenInfo beginMetaParametersToken, const lib::String& code)
{
	SPT_CHECK_NO_ENTRY();

	const SizeType semicolonPosition	= code.find(';', beginMetaParametersToken.tokenPosition);
	const SizeType openBracketPosition	= code.find('{', beginMetaParametersToken.tokenPosition);
	SPT_CHECK(semicolonPosition != lib::String::npos && openBracketPosition != lib::String::npos);

	const Bool isBufferBlock = openBracketPosition < semicolonPosition;

	lib::HashedString paramName;

	if (isBufferBlock)
	{
		const SizeType reverseFindBeginOffset		= code.size() - openBracketPosition;
		const SizeType reverseFindEndOffset			= code.size() - beginMetaParametersToken.tokenPosition;
		const auto nameLastCharacterIt				= std::find_if_not(std::crbegin(code) + reverseFindBeginOffset,
																	   std::crbegin(code) + reverseFindEndOffset,
																	   [](char c) { return std::isspace(c); });
		const SizeType nameLastCharacterPosition	= std::distance(code.data(), &*nameLastCharacterIt);
		const SizeType whiteCharBeforeNamePosition	= code.find_last_of(" \t", nameLastCharacterPosition);

		const SizeType nameBeginPosition			= whiteCharBeforeNamePosition + 1;
		const SizeType nameEndPosition				= nameLastCharacterPosition + 1;
		const SizeType nameLength					= nameEndPosition - nameBeginPosition;
		paramName									= lib::StringView(code.data() + nameBeginPosition, nameLength);
	}
	else
	{
		// This code assumes that there can be only array size between variable name and semicolon (no whitespaces)!
		SPT_CHECK(std::isspace(code[semicolonPosition - 1]));

		const SizeType whiteCharBeforeNamePosition	= code.find_last_of(" \t", semicolonPosition);
		const SizeType nameBeginPosition			= whiteCharBeforeNamePosition + 1;
		const SizeType nameEndPosition				= code.find_first_of("[;", nameBeginPosition);
		const SizeType nameLength					= nameEndPosition - nameBeginPosition;
		paramName									= lib::StringView(code.data() + nameBeginPosition, nameLength);
	}

	return paramName;
}

static lib::StringView ExtractMetaParametersString(tkn::TokenInfo beginMetaParametersToken, const lib::String& code, SizeType& metaDataEndPosition)
{
	SPT_CHECK(beginMetaParametersToken.IsValid());

	const lib::StringView metaParametersString = tkn::TokenizerUtils::GetStringInNearestBracket(code, beginMetaParametersToken.tokenPosition);

	// add 1 and the end because we need to also add closing bracket (it's not included in metaParametersString)
	metaDataEndPosition = std::distance(code.data(), metaParametersString.data()) + metaParametersString.size() + 1;

	return metaParametersString;
}

static void BuildParameterMetaData(lib::HashedString paramName, lib::StringView metaParametersString, ShaderCompilationMetaData& outMetaData)
{
	SPT_CHECK_NO_ENTRY();

	const lib::DynamicArray<lib::StringView> arguments = tkn::ArgumentsTokenizer::ExtractArguments(metaParametersString);

	ShaderParamMetaData paramMetaData;
	for (const lib::StringView argumentView : arguments)
	{
		paramMetaData.AddMeta(lib::HashedString(argumentView));
	}
	
	outMetaData.AddParamMetaData(paramName, std::move(paramMetaData));
}


enum class EDSIteratorFuncResult
{
	Continue,
	Break,
	ContinueFromStart
};


template<typename TFunctor>
static void IterateDescriptorSets(TFunctor&& func, const lib::String& sourceCode)
{
	static const std::regex descriptorSetRegex(R"~(\[\[descriptor_set\((\w+)\s*,\s*(\d+)\s*\)\]\])~");

	auto descriptorSetIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), descriptorSetRegex);

	while (descriptorSetIt != std::sregex_iterator())
	{
		const std::smatch descriptorSetMatch = *descriptorSetIt;
		SPT_CHECK(descriptorSetMatch.size() == 3); // should be whole match + dsNameMatch + dsIdxMatch
		const lib::HashedString dsName = descriptorSetMatch[1].str();
		const lib::String dsIdxStr = descriptorSetMatch[2].str();
		const Uint32 dsIdx = static_cast<Uint32>(std::stoi(dsIdxStr));

		const SizeType dsTokenPosition = descriptorSetIt->prefix().length();
		const SizeType dsTokenLength   = descriptorSetMatch.length();

		const EDSIteratorFuncResult res = func(dsName, dsIdx, dsTokenPosition, dsTokenLength);

		if (res == EDSIteratorFuncResult::Break)
		{
			break;
		}
		else if (res == EDSIteratorFuncResult::Continue)
		{
			++descriptorSetIt;
		}
		else if (res == EDSIteratorFuncResult::ContinueFromStart)
		{
			descriptorSetIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), descriptorSetRegex);
		}
		else
		{
			SPT_CHECK_NO_ENTRY();
		}
	}
}

} // helper

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderMetaDataPrerpocessor ====================================================================

ShaderPreprocessingMetaData ShaderMetaDataPrerpocessor::PreprocessAdditionalCompilerArgs(const lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderPreprocessingMetaData metaData;

	helper::IterateDescriptorSets([&metaData](const lib::HashedString& dsName, Uint32 dsIdx, SizeType dsTokenPosition, SizeType dsTokenLength)
								  {
									  const DescriptorSetCompilationDef& dsCompilationDef = DescriptorSetCompilationDefsRegistry::GetDescriptorSetCompilationDef(dsName);
									  std::copy(std::cbegin(dsCompilationDef.GetMetaData().additionalMacros),
												std::cend(dsCompilationDef.GetMetaData().additionalMacros),
												std::back_inserter(metaData.macroDefinitions));
									  return helper::EDSIteratorFuncResult::Continue;
								  },
								  sourceCode);

	return metaData;
}

ShaderCompilationMetaData ShaderMetaDataPrerpocessor::PreprocessShader(lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderCompilationMetaData metaData;

	PreprocessShaderDescriptorSets(sourceCode, OUT metaData);
	PreprocessShaderStructs(sourceCode, OUT metaData);
	PreprocessShaderParametersMetaData(sourceCode, OUT metaData);

#if SPT_SHADERS_DEBUG_FEATURES
	PreprocessShaderLiterals(sourceCode, OUT metaData);
#endif // SPT_SHADERS_DEBUG_FEATURES

	return metaData;
}

void ShaderMetaDataPrerpocessor::PreprocessShaderStructs(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex shaderStructRegex(R"~(\[\[shader_struct\(\s*(\w*)\s*\)\]\])~");

	lib::HashSet<lib::HashedString> m_definedStructs;

	auto shaderStructsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderStructRegex);

	while (shaderStructsIt != std::sregex_iterator())
	{
		const std::smatch shaderStructMatch = *shaderStructsIt;
		SPT_CHECK(shaderStructMatch.size() == 2); // should be whole match + structNameMatch
		const lib::HashedString structName = shaderStructMatch[1].str();

		lib::String structSourceCode;
		if (!m_definedStructs.contains(structName))
		{
			const ShaderStructDefinition& structDef = ShaderStructsRegistry::GetStructDefinition(structName);
			structSourceCode = structDef.GetSourceCode();

			m_definedStructs.emplace(structName);
		}

		sourceCode.replace(shaderStructsIt->prefix().length(), shaderStructMatch.length(), structSourceCode);

		// Always search for new struct from the beginning, because we're modifying source code during loop
		shaderStructsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderStructRegex);
	}
}

void ShaderMetaDataPrerpocessor::PreprocessShaderDescriptorSets(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();
	
	helper::IterateDescriptorSets([&sourceCode, &outMetaData](const lib::HashedString& dsName, Uint32 dsIdx, SizeType dsTokenPosition, SizeType dsTokenLength)
								  {
									  const DescriptorSetCompilationDef& dsCompilationDef = DescriptorSetCompilationDefsRegistry::GetDescriptorSetCompilationDef(dsName);

									  const lib::String dsSourceCode = dsCompilationDef.GetShaderCode(dsIdx);
									  sourceCode.replace(dsTokenPosition, dsTokenLength, dsSourceCode);

									  outMetaData.AddDescriptorSetMetaData(dsIdx, dsCompilationDef.GetMetaData());
									  
									  return helper::EDSIteratorFuncResult::ContinueFromStart;
								  },
								  sourceCode);
}

void ShaderMetaDataPrerpocessor::PreprocessShaderParametersMetaData(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	const tkn::Tokenizer tokenizer(sourceCode, g_metaDataProcessorTokens);
	const tkn::TokensArray tokensArray = tokenizer.BuildTokensArray();

	tkn::TokensVisitor metaDataVisitor;
	metaDataVisitor.BindToToken(EMetaDataProcessorToken::BeginMetaData).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			const lib::HashedString paramName = helper::ExtractParameterName(tokenInfo, sourceCode);
			SPT_CHECK(paramName.IsValid());

			SizeType metaDataEndPosition = 0;
			const lib::StringView metaParametersString = helper::ExtractMetaParametersString(tokenInfo, sourceCode, metaDataEndPosition);
			SPT_CHECK(metaDataEndPosition != 0);

			helper::BuildParameterMetaData(paramName, metaParametersString, outMetaData);

			// fill whole meta data with whitespaces, so it won't be compiled
			std::fill(std::begin(sourceCode) + tokenInfo.tokenPosition, std::begin(sourceCode) + metaDataEndPosition, ' ');
		});

	const tkn::TokensProcessor tokensProcessor(tokensArray);
	tokensProcessor.VisitAllTokens(metaDataVisitor);
}

#if SPT_SHADERS_DEBUG_FEATURES
void ShaderMetaDataPrerpocessor::PreprocessShaderLiterals(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex shaderLiteralRegex(R"~(L"[^"]*")~");

	auto shaderLiteralsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderLiteralRegex);

	while (shaderLiteralsIt != std::sregex_iterator())
	{
		const std::smatch literalMatch = *shaderLiteralsIt;
		SPT_CHECK(literalMatch.size() == 1);
		const lib::String matchString = literalMatch[0].str();
		const lib::StringView literalString(matchString.cbegin() + 2, matchString.cbegin() + matchString.length() - 1);
		const lib::HashedString literal = literalString;

		const Uint64 literalHash = static_cast<Uint64>(literal.GetKey());

		const Uint32 literalLow = static_cast<Uint32>(literalHash & 0xFFFFFFFF);
		const Uint32 literalHigh = static_cast<Uint32>(literalHash >> 32);

		const lib::String literalShaderCode = std::format("debug::CreateLiteral(uint2({}, {}))", literalLow, literalHigh);

		sourceCode.replace(shaderLiteralsIt->prefix().length(), literalMatch.length(), literalShaderCode);

		// Always search for new struct from the beginning, because we're modifying source code during loop
		shaderLiteralsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderLiteralRegex);

		outMetaData.AddDebugLiteral(literal);
	}
}
#endif // SPT_SHADERS_DEBUG_FEATURES

} // spt::sc
