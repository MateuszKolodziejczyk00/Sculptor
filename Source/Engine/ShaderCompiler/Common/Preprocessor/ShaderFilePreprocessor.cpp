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

static ECompiledShaderType StringToShaderType(lib::StringView typeName)
{
	if (typeName == "vertex")
	{
		return ECompiledShaderType::Vertex;
	}
	else if(typeName == "fragment")
	{
		return ECompiledShaderType::Fragment;
	}
	else if(typeName == "compute")
	{
		return ECompiledShaderType::Compute;
	}

	SPT_CHECK_NO_ENTRY();
	return ECompiledShaderType::None;
}

} // internal

//////////////////////////////////////////////////////////////////////////////////////////////////
// PreprocessedShadersGroup ======================================================================

PreprocessedShadersGroup::PreprocessedShadersGroup(lib::HashedString groupName)
	: m_groupName(groupName)
{
	SPT_CHECK(m_groupName.IsValid());
}

Bool PreprocessedShadersGroup::IsValid() const
{
	return std::all_of(std::cbegin(m_shaders), std::cend(m_shaders), [](const ShaderSourceCode& shaderCode) { return shaderCode.IsValid(); });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderFilePreprocessingResult =================================================================

Bool ShaderFilePreprocessingResult::IsValid() const
{
	return std::all_of(std::cbegin(m_shadersGroups), std::cend(m_shadersGroups), [](const PreprocessedShadersGroup& group) { return group.IsValid(); });
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
			SPT_CHECK(!result.m_shadersGroups.empty());

			PreprocessedShadersGroup& currentShadersGroup = result.m_shadersGroups.back();

			const lib::StringView shaderCode = tokenizer.GetStringBetweenTokens(tokenInfo, processor.FindNextToken(tokenInfo));

			const lib::StringView shaderTypeName = tkn::TokenizerUtils::GetStringInNearestBracket(shaderCode);

			ShaderSourceCode sourceCode(currentShadersGroup.m_groupName);
			sourceCode.SetSourceCode(lib::String(shaderCode));
			sourceCode.SetShaderType(internal::StringToShaderType(shaderTypeName));

			currentShadersGroup.m_shaders.emplace_back(sourceCode);
		});

	tkn::TokensVisitor groupsVisitor;
	groupsVisitor.BindToToken(EToken::BeginShadersGroup).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			const tkn::TokenInfo nextGroupToken = processor.FindNextTokenOfType(tokenInfo, EToken::BeginShadersGroup);

			const lib::StringView groupCode = tokenizer.GetStringBetweenTokens(tokenInfo, nextGroupToken);

			const lib::HashedString currentGroupName = tkn::TokenizerUtils::GetStringInNearestBracket(groupCode);

			result.m_shadersGroups.emplace_back(PreprocessedShadersGroup(currentGroupName));

			processor.VisitTokens(shadersVisitor, tokenInfo, nextGroupToken);
		});

	tkn::TokensProcessor tokensProcessor(tokensArray);
	tokensProcessor.VisitAllTokens(groupsVisitor);

	return result;
}

} // spt::sc
