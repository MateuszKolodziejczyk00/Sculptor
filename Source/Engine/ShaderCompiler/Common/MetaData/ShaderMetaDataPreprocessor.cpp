#include "ShaderMetaDataPreprocessor.h"
#include "Common/ShaderCompilationInput.h"
#include "Tokenizer.h"
#include "ArgumentsTokenizer.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefsRegistry.h"

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

static void BuildParameterMetaData(lib::HashedString paramName, lib::StringView metaParametersString, ShaderParametersMetaData& outMetaData)
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

} // helper

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderMetaDataPrerpocessor ====================================================================

ShaderParametersMetaData ShaderMetaDataPrerpocessor::PreprocessShader(ShaderSourceCode& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderParametersMetaData metaData;

	PreprocessShaderDescriptorSets(sourceCode, metaData);
	//PreprocessShaderParametersMetaData(sourceCode, metaData);

	return metaData;
}

void ShaderMetaDataPrerpocessor::PreprocessShaderDescriptorSets(ShaderSourceCode& sourceCode, ShaderParametersMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	lib::String& sourceCodeStr = sourceCode.GetSourceCodeMutable();

	static const std::regex descriptorSetRegex(R"~(\[\[descriptor_set\((\w+)\s*,\s*(\d)\s*\)\]\])~");

	auto descriptorSetIt = std::sregex_iterator(std::cbegin(sourceCodeStr), std::cend(sourceCodeStr), descriptorSetRegex);

	while (descriptorSetIt != std::sregex_iterator())
	{
		const std::smatch descriptorSetMatch = *descriptorSetIt;
		SPT_CHECK(descriptorSetMatch.size() == 3); // should be whole match + dsNameMatch + dsIdxMatch
		const lib::HashedString dsName = descriptorSetMatch[1].str();
		const lib::String dsIdxStr = descriptorSetMatch[2].str();
		const Uint32 dsIdx = static_cast<Uint32>(std::stoi(dsIdxStr));

		const DescriptorSetCompilationDef& dsCompilationDef = DescriptorSetCompilationDefsRegistry::GetDescriptorSetCompilationDef(dsName);

		const lib::String dsSourceCode = dsCompilationDef.GetShaderCode(dsIdx);
		sourceCodeStr.replace(descriptorSetIt->prefix().length(), descriptorSetMatch.length(), dsSourceCode);

		outMetaData.AddDescriptorSetMetaData(dsIdx, dsCompilationDef.GetMetaData());

		// Always search for new descriptor from the beginning, because we're modifying source code during loop
		descriptorSetIt = std::sregex_iterator(std::cbegin(sourceCodeStr), std::cend(sourceCodeStr), descriptorSetRegex);
	}
}

void ShaderMetaDataPrerpocessor::PreprocessShaderParametersMetaData(ShaderSourceCode& sourceCode, ShaderParametersMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	lib::String& code = sourceCode.GetSourceCodeMutable();

	const tkn::Tokenizer tokenizer(code, g_metaDataProcessorTokens);
	const tkn::TokensArray tokensArray = tokenizer.BuildTokensArray();

	tkn::TokensVisitor metaDataVisitor;
	metaDataVisitor.BindToToken(EMetaDataProcessorToken::BeginMetaData).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			const lib::HashedString paramName = helper::ExtractParameterName(tokenInfo, code);
			SPT_CHECK(paramName.IsValid());

			SizeType metaDataEndPosition = 0;
			const lib::StringView metaParametersString = helper::ExtractMetaParametersString(tokenInfo, code, metaDataEndPosition);
			SPT_CHECK(metaDataEndPosition != 0);

			helper::BuildParameterMetaData(paramName, metaParametersString, outMetaData);

			// fill whole meta data with whitespaces, so it won't be compiled
			std::fill(std::begin(code) + tokenInfo.tokenPosition, std::begin(code) + metaDataEndPosition, ' ');
		});

	const tkn::TokensProcessor tokensProcessor(tokensArray);
	tokensProcessor.VisitAllTokens(metaDataVisitor);
}

} // spt::sc
