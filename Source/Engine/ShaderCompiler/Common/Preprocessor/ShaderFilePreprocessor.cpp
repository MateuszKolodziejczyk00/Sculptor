#include "ShaderFilePreprocessor.h"
#include "RHI/RHICore/RHIShaderTypes.h"

#include <regex>

namespace spt::sc
{

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
// ShaderFilePreprocessingResult =================================================================

Bool ShaderFilePreprocessingResult::IsValid() const
{
	return !shaders.empty() && std::all_of(std::cbegin(shaders), std::cend(shaders), [](const ShaderSourceCode& shaderCode) { return shaderCode.IsValid(); });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderFilePreprocessor ========================================================================

ShaderFilePreprocessingResult ShaderFilePreprocessor::PreprocessShaderFileSourceCode(const lib::String& sourceCode)
{
	SPT_PROFILE_FUNCTION();

	ShaderFilePreprocessingResult result;
	
	const std::regex shaderTypeRegex("#type\\(\\s*\\w\\s*\\)");
	const std::regex shaderTypeNameRegex("(?=\\(\\s*)\\w");

	auto shadersBeginMacros = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderTypeRegex);

	SPT_CHECK(shadersBeginMacros != std::sregex_iterator());

	while (shadersBeginMacros != std::sregex_iterator())
	{
		auto nextShaderBegin = shadersBeginMacros;
		nextShaderBegin++;

		const std::smatch typeMacroMatch = *shadersBeginMacros;
		const lib::String typeMacroMatchString = typeMacroMatch.str();

		std::smatch typeNameMatch;
		const Bool found = std::regex_search(typeMacroMatchString, typeNameMatch, shaderTypeNameRegex);
		SPT_CHECK(found);

		const lib::String shaderTypeName = typeNameMatch.str();
		const rhi::EShaderStage shaderStage = internal::StringToShaderType(shaderTypeName);

		const lib::String shaderSourceCodeString = nextShaderBegin != std::sregex_iterator()
			? lib::String(std::cbegin(sourceCode) + (sourceCode.size() - shadersBeginMacros->suffix().length()), std::cbegin(sourceCode) + nextShaderBegin->prefix().length())
			: lib::String(std::cbegin(sourceCode) + (sourceCode.size() - shadersBeginMacros->suffix().length()), std::cbegin(sourceCode) + sourceCode.size());

		result.shaders.emplace_back(std::move(shaderSourceCodeString), shaderStage);

		shadersBeginMacros = nextShaderBegin;
	}

	return result;
}

} // spt::sc
