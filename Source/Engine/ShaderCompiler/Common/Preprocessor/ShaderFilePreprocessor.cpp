#include "ShaderFilePreprocessor.h"
#include "Tokenizer.h"

namespace spt::sc
{

namespace EToken
{

enum Type : Uint32
{
	BeginShadersGroup,
	beginShader,

	NUM
};

}

static const lib::StaticArray<tkn::Token, EToken::NUM> tokens
{
	tkn::Token("#begin("),
	tkn::Token("#type(")
};


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

}


ShaderFilePreprocessingResult ShaderFilePreprocessor::PreprocessShaderFileSourceCode(const lib::String& sourceCode)
{
	SPT_PROFILE_FUNCTION();

	ShaderFilePreprocessingResult result;

	const tkn::Tokenizer tokenizer(sourceCode, tokens);

	const tkn::TokensArray tokens = tokenizer.BuildTokensArray();

	lib::StringView currentGroupName;
	
	tkn::TokensVisitor shadersVisitor;
	shadersVisitor.BindToToken(EToken::beginShader).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			const lib::StringView shaderCode = tokenizer.GetStringBetweenTokens(tokenInfo, processor.FindNextToken(tokenInfo));

			const lib::StringView shaderTypeName = tkn::TokenizerUtils::GetStringInNearestBracket(shaderCode);

			ShaderSourceCode sourceCode(currentGroupName);
			sourceCode.SetSourceCode(lib::String(shaderCode));
			sourceCode.SetShaderType(internal::StringToShaderType(shaderTypeName));

			result.m_shadersGroups.back().m_shaders.emplace_back(sourceCode);
		});

	tkn::TokensVisitor groupsVisitor;
	groupsVisitor.BindToToken(EToken::BeginShadersGroup).BindLambda(
		[&](tkn::TokenInfo tokenInfo, const tkn::TokensProcessor& processor)
		{
			const tkn::TokenInfo nextGroupToken = processor.FindNextTokenOfType(tokenInfo, EToken::BeginShadersGroup);

			const lib::StringView groupCode = tokenizer.GetStringBetweenTokens(tokenInfo, nextGroupToken);

			currentGroupName = tkn::TokenizerUtils::GetStringInNearestBracket(groupCode);

			result.m_shadersGroups.emplace_back(PreprocessedShadersGroup());

			processor.VisitTokens(shadersVisitor, tokenInfo, nextGroupToken);
		});

	tkn::TokensProcessor tokensProcessor(tokens);
	tokensProcessor.VisitAllTokens(groupsVisitor);

	return result;
}

}
