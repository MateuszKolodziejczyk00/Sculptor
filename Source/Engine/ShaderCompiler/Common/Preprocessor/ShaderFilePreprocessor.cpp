#include "ShaderFilePreprocessor.h"
#include "Tokenizer.h"

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Token =========================================================================================

namespace EToken
{

enum Type : Uint32
{
	BeginShadersGroup,
	beginShader,

	NUM
};

} // EToken

static const lib::StaticArray<tkn::Token, EToken::NUM> tokens
{
	tkn::Token("#begin("),
	tkn::Token("#type(")
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// internal ======================================================================================

namespace internal
{

static rhi::EShaderStage StringToShaderType(lib::StringView stageName)
{
	if (stageName == "vertex")
	{
		return rhi::EShaderStage::Vertex;
	}
	else if(stageName == "fragment")
	{
		return rhi::EShaderStage::Fragment;
	}
	else if(stageName == "compute")
	{
		return rhi::EShaderStage::Compute;
	}

	SPT_CHECK_NO_ENTRY();
	return rhi::EShaderStage::None;
}

} // internal

//////////////////////////////////////////////////////////////////////////////////////////////////
// PreprocessedShadersGroup ======================================================================

PreprocessedShadersGroup::PreprocessedShadersGroup(lib::HashedString inGroupName)
	: groupName(inGroupName)
{
	SPT_CHECK(groupName.IsValid());
}

Bool PreprocessedShadersGroup::IsValid() const
{
	return std::all_of(std::cbegin(shaders), std::cend(shaders), [](const ShaderSourceCode& shaderCode) { return shaderCode.IsValid(); });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderFilePreprocessingResult =================================================================

Bool ShaderFilePreprocessingResult::IsValid() const
{
	return std::all_of(std::cbegin(shadersGroups), std::cend(shadersGroups), [](const PreprocessedShadersGroup& group) { return group.IsValid(); });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderFilePreprocessor ========================================================================

ShaderFilePreprocessingResult ShaderFilePreprocessor::PreprocessShaderFileSourceCode(const lib::String& sourceCode)
{
	SPT_PROFILE_FUNCTION();

	ShaderFilePreprocessingResult result;

	const tkn::Tokenizer tokenizer(sourceCode, tokens);

	const tkn::TokensArray tokensArray = tokenizer.BuildTokensArray();
	
	tkn::TokensVisitor shadersVisitor;
	shadersVisitor.BindToToken(EToken::beginShader).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			SPT_CHECK(!result.shadersGroups.empty());

			PreprocessedShadersGroup& currentShadersGroup = result.shadersGroups.back();

			const lib::StringView shaderCode = tokenizer.GetStringBetweenTokens(tokenInfo, processor.FindNextToken(tokenInfo));

			const lib::StringView shaderTypeName = tkn::TokenizerUtils::GetStringInNearestBracket(shaderCode, 0);

			ShaderSourceCode sourceCode(currentShadersGroup.groupName);
			sourceCode.SetSourceCode(lib::String(shaderCode));
			sourceCode.SetShaderStage(internal::StringToShaderType(shaderTypeName));

			currentShadersGroup.shaders.emplace_back(sourceCode);
		});

	tkn::TokensVisitor groupsVisitor;
	groupsVisitor.BindToToken(EToken::BeginShadersGroup).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			const tkn::TokenInfo nextGroupToken = processor.FindNextTokenOfType(tokenInfo, EToken::BeginShadersGroup);

			const lib::StringView groupCode = tokenizer.GetStringBetweenTokens(tokenInfo, nextGroupToken);

			const lib::HashedString currentGroupName = tkn::TokenizerUtils::GetStringInNearestBracket(groupCode, 0);

			result.shadersGroups.emplace_back(PreprocessedShadersGroup(currentGroupName));

			processor.VisitTokens(shadersVisitor, tokenInfo, nextGroupToken);
		});

	const tkn::TokensProcessor tokensProcessor(tokensArray);
	tokensProcessor.VisitAllTokens(groupsVisitor);

	return result;
}

} // spt::sc
