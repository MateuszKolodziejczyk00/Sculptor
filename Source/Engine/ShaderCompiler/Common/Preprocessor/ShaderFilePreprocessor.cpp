#include "ShaderFilePreprocessor.h"
#include "RHI/RHICore/RHIShaderTypes.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefsRegistry.h"

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
	SPT_PROFILER_FUNCTION();

	ShaderFilePreprocessingResult result;
	
	static const std::regex shaderTypeRegex(R"~(#type\(\s*\w+\s*\))~");
	static const std::regex shaderTypeNameRegex(R"~(\w+(?=\s*\)))~");

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

		lib::String shaderSourceCodeString = nextShaderBegin != std::sregex_iterator()
			? lib::String(std::cbegin(sourceCode) + (sourceCode.size() - shadersBeginMacros->suffix().length()), std::cbegin(sourceCode) + nextShaderBegin->prefix().length())
			: lib::String(std::cbegin(sourceCode) + (sourceCode.size() - shadersBeginMacros->suffix().length()), std::cbegin(sourceCode) + sourceCode.size());

		PreprocessShaderSourceCode(shaderSourceCodeString);

		result.shaders.emplace_back(std::move(shaderSourceCodeString), shaderStage);

		shadersBeginMacros = nextShaderBegin;
	}

	return result;
}

void ShaderFilePreprocessor::PreprocessShaderSourceCode(lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex descriptorSetRegex(R"~(\[\[descriptor_set\((\w+)\s*,\s*(\d)\s*\)\]\])~");

	auto descriptorSetIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), descriptorSetRegex);

	while (descriptorSetIt != std::sregex_iterator())
	{
		const std::smatch descriptorSetMatch = *descriptorSetIt;
		SPT_CHECK(descriptorSetMatch.size() == 3); // should be whole match + dsNameMatch + dsIdxMatch
		const lib::HashedString dsName = descriptorSetMatch[1].str();
		const lib::String dsIdxStr = descriptorSetMatch[2].str();
		const Uint32 dsIdx = static_cast<Uint32>(std::stoi(dsIdxStr));

		const lib::String dsSourceCode = DescriptorSetCompilationDefsRegistry::GetDescriptorSetShaderSourceCode(dsName, dsIdx);
		sourceCode.replace(descriptorSetIt->prefix().length(), descriptorSetMatch.length(), dsSourceCode);

		descriptorSetIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), descriptorSetRegex);
	}
}

} // spt::sc
