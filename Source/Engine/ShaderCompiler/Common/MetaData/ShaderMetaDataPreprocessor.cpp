#include "ShaderMetaDataPreprocessor.h"
#include "ShaderStructsRegistry.h"
#include "FileSystem/File.h"
#include "Utility/String/StringUtils.h"

#include <regex>


SPT_DEFINE_LOG_CATEGORY(ShaderMetaDataPrerpocessor, true)


namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace helper
{

static void PreprocessShaderMetaParameters(const lib::String& sourceCode, ShaderPreprocessingMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	const lib::HashMap<lib::HashedString, lib::HashedString> m_definedParams =
	{
		{"debug_features", "SPT_META_PARAM_DEBUG_FEATURES"}
	};

	static const std::regex metaDataRegex(R"~(\[\[meta\((.*?)\)\]\])~");
	std::smatch match;

	if (std::regex_search(sourceCode, match, metaDataRegex))
	{
		const std::string params = match[1];
		const std::regex paramRegex(",\\s*");

		std::sregex_token_iterator iter(params.begin(), params.end(), paramRegex, -1);
		const std::sregex_token_iterator end;

		for (; iter != end; ++iter)
		{
			const lib::HashedString param = std::string(*iter);
			if (param.IsValid())
			{
				const auto paramMacroDef = m_definedParams.find(param);
				if (paramMacroDef != m_definedParams.cend())
				{
					outMetaData.macroDefinitions.emplace_back(paramMacroDef->second);
				}
				else if (param == lib::StringView("force_debug_mode"))
				{
					outMetaData.forceDebugMode = true;
				}
				else
				{
					SPT_LOG_ERROR(ShaderMetaDataPrerpocessor, "Unknown meta parameter: {}", param.GetView());
				}
			}
		}
	}
}

static void RemoveMetaParameters(lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex metaDataRegex(R"~(\[\[meta\((.*?)\)\]\])~");
	sourceCode = std::regex_replace(sourceCode, metaDataRegex, "");
}

static void PreprocessShaderStructs(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex shaderStructRegex(R"~(\[\[shader_struct\(\s*(\w*)\s*\)\]\])~");

	lib::HashSet<lib::HashedString> definedStructs;

	auto shaderStructsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderStructRegex);

	while (shaderStructsIt != std::sregex_iterator())
	{
		const std::smatch& shaderStructMatch = *shaderStructsIt;
		SPT_CHECK(shaderStructMatch.size() == 2); // should be whole match + structNameMatch
		const lib::String structName = shaderStructMatch[1].str();

		lib::String structSourceCode;
		if (!definedStructs.contains(structName))
		{
			const rdr::ShaderStructMetaData* structMetaData = rdr::ShaderStructsRegistry::GetStructMetaData(structName);
			if (!structMetaData)
			{
				SPT_LOG_ERROR(ShaderMetaDataPrerpocessor, "Shader struct '{}' is not registered in ShaderStructsRegistry", structName);
				structSourceCode = "struct " + structName + " { ??? };";
			}
			else
			{
				structSourceCode = structMetaData->GetHLSLSourceCode();
			}

#if WITH_SHADERS_HOT_RELOAD
			outMetaData.shaderStructsVersionHashes[structName] = structMetaData ? structMetaData->GetVersionHash() : 0u;
#endif // WITH_SHADERS_HOT_RELOAD

			definedStructs.emplace(structName);
		}

		sourceCode.replace(shaderStructsIt->prefix().length(), shaderStructMatch.length(), structSourceCode);

		// Always search for new struct from the beginning, because we're modifying source code during loop
		shaderStructsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderStructRegex);
	}
}

static void PreprocessShaderParams(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex pushConstantRegex(R"~(\[\[shader_params\(\s*(\w*)\s*\,\s*(\w*)\s*\)\]\])~");

	auto pushConstantIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), pushConstantRegex);

	struct ShaderParamInfo
	{
		lib::HashedString structName;
		lib::HashedString variableName;
	};

	lib::InlineDynamicArray<ShaderParamInfo, 16u> shaderParams;

	while (pushConstantIt != std::sregex_iterator())
	{
		const std::smatch& pushConstantMatch = *pushConstantIt;
		SPT_CHECK(pushConstantMatch.size() == 3); // [{whole math}, {struct type}, {name}]

		const lib::HashedString structName   = pushConstantMatch[1].str();
		const lib::HashedString variableName = pushConstantMatch[2].str();

		shaderParams.EmplaceBack(ShaderParamInfo{ structName, variableName });

		++pushConstantIt;
	}

	if (!shaderParams.IsEmpty())
	{
		lib::String generatedCode;
		for (const ShaderParamInfo& shaderParam : shaderParams)
		{
			generatedCode += "[[shader_struct(";
			generatedCode += shaderParam.structName.GetView();
			generatedCode += ")]]\n";
		}

		generatedCode += "struct GENERATED_SHADER_PARAMS\n{\n";
		for (const ShaderParamInfo& shaderParam : shaderParams)
		{
			generatedCode += "    ";
			generatedCode += shaderParam.structName.GetView();
			generatedCode += "* _";
			generatedCode += shaderParam.variableName.GetView();
			generatedCode += ";\n";

			outMetaData.AddShaderParam(shaderParam.structName);
		}

		generatedCode += "};\n";

		generatedCode += "[[vk::push_constant]] GENERATED_SHADER_PARAMS __shaderParams;\n";

		sourceCode += generatedCode;

		lib::String accessorsCode;
		for (const ShaderParamInfo& shaderParam : shaderParams)
		{
			accessorsCode += "#define ";
			accessorsCode += shaderParam.variableName.GetView();
			accessorsCode += " (__shaderParams._";
			accessorsCode += shaderParam.variableName.GetView();
			accessorsCode += ")\n";
		}

		sourceCode.insert(sourceCode.begin(), accessorsCode.cbegin(), accessorsCode.cend());
	}
}

#if WITH_SHADERS_HOT_RELOAD
static void PreprocessFileDependencies(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex includeFileRegex(R"~(#line.*\"(.*)\")~");

	for (std::sregex_iterator it(std::cbegin(sourceCode), std::cend(sourceCode), includeFileRegex);
			it != std::sregex_iterator();
			++it)
	{
		const std::smatch& includeFileMatch = *it;
		SPT_CHECK(includeFileMatch.size() == 2);

		lib::String filePath = includeFileMatch[1].str();

		if (lib::Path(filePath).is_absolute())
		{
			outMetaData.AddFileDependencyUnique(std::move(filePath));
		}
	}
}
#endif // WITH_SHADERS_HOT_RELOAD

#if SPT_SHADERS_DEBUG_FEATURES
static void PreprocessShaderLiterals(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex shaderLiteralRegex(R"~(L"[^"]*")~");

	auto shaderLiteralsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderLiteralRegex);

	while (shaderLiteralsIt != std::sregex_iterator())
	{
		const std::smatch& literalMatch = *shaderLiteralsIt;
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

} // helper

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderMetaDataPrerpocessor ====================================================================

ShaderPreprocessingMetaData ShaderMetaDataPrerpocessor::PreprocessMainShaderFile(const lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderPreprocessingMetaData metaData;

	helper::PreprocessShaderMetaParameters(sourceCode, INOUT metaData);

	return metaData;
}

ShaderPreprocessingMetaData ShaderMetaDataPrerpocessor::PreprocessAdditionalCompilerArgs(const lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderPreprocessingMetaData metaData;

	static const std::regex pushConstantRegex(R"~(\[\[shader_params\(\s*(\w*)\s*\,\s*(\w*)\s*\)\]\])~");

	auto pushConstantIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), pushConstantRegex);

	while (pushConstantIt != std::sregex_iterator())
	{
		const std::smatch& pushConstantMatch = *pushConstantIt;
		SPT_CHECK(pushConstantMatch.size() == 3); // [{whole math}, {struct type}, {name}]

		const lib::HashedString structName   = pushConstantMatch[1].str();
		const lib::HashedString variableName = pushConstantMatch[2].str();

		metaData.macroDefinitions.emplace_back(lib::String("PARAM_") + structName.ToString() + "=(__shaderParams._" + variableName.ToString() + ")");

		++pushConstantIt;
	}

	return metaData;
}

ShaderCompilationMetaData ShaderMetaDataPrerpocessor::PreprocessShader(lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderCompilationMetaData metaData;

	helper::PreprocessShaderParams(sourceCode, OUT metaData);
	helper::PreprocessShaderStructs(sourceCode, OUT metaData);

	helper::RemoveMetaParameters(sourceCode);

#if WITH_SHADERS_HOT_RELOAD
	helper::PreprocessFileDependencies(sourceCode, OUT metaData);
#endif // WITH_SHADERS_HOT_RELOAD

#if SPT_SHADERS_DEBUG_FEATURES
	helper::PreprocessShaderLiterals(sourceCode, OUT metaData);
#endif // SPT_SHADERS_DEBUG_FEATURES

	return metaData;
}

} // spt::sc
