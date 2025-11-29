#include "ShaderMetaDataPreprocessor.h"
#include "Common/ShaderCompilationInput.h"
#include "Tokenizer.h"
#include "ArgumentsTokenizer.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefsRegistry.h"
#include "ShaderStructsRegistry.h"
#include "FileSystem/File.h"

#include <regex>
#include "Utility/String/StringUtils.h"

SPT_DEFINE_LOG_CATEGORY(ShaderMetaDataPrerpocessor, true)

#pragma optimize("", off)
namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace helper
{

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
		const Uint32 bindlessOffset = 1u;
		const Uint32 dsIdx = static_cast<Uint32>(std::stoi(dsIdxStr)) + bindlessOffset;

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


static TypeOverrideMap ParseTypeOverrides(lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	TypeOverrideMap overrides;

	const lib::StringView overrideToken = "[[override]]";

	SizeType currentPos = 0u;
	while (true)
	{
		currentPos = sourceCode.find(overrideToken, currentPos);

		if (currentPos != lib::String::npos)
		{
			sourceCode.replace(currentPos, overrideToken.length(), ""); // Remove [[override]] token to simplify parsing

			while (currentPos < sourceCode.length() && lib::StringUtils::IsWhiteChar(sourceCode[currentPos]))
			{
				++currentPos;
			}

			const SizeType structTokenPos = currentPos;

			const lib::StringView structTokenView = "struct";
			SPT_CHECK(sourceCode.compare(currentPos, structTokenView.length(), structTokenView) == 0);
			currentPos += structTokenView.length();

			while (currentPos < sourceCode.length() && lib::StringUtils::IsWhiteChar(sourceCode[currentPos]))
			{
				++currentPos;
			}

			SizeType structNameStartPos = currentPos;

			while(currentPos < sourceCode.length() && !lib::StringUtils::IsWhiteChar(sourceCode[currentPos]) && sourceCode[currentPos] != '{' && sourceCode[currentPos] != ':')
			{
				++currentPos;
			}

			const lib::StringView overrideStructName = lib::StringView(&sourceCode[structNameStartPos], currentPos - structNameStartPos);

			while (currentPos < sourceCode.length() && lib::StringUtils::IsWhiteChar(sourceCode[currentPos]))
			{
				++currentPos;
			}

			SPT_CHECK(sourceCode[currentPos] == ':');

			while (currentPos < sourceCode.length() && (lib::StringUtils::IsWhiteChar(sourceCode[currentPos]) || sourceCode[currentPos] == ':'))
			{
				++currentPos;
			}

			SizeType originalStructNameStartPos = currentPos;

			while (currentPos < sourceCode.length() && !lib::StringUtils::IsWhiteChar(sourceCode[currentPos]) && sourceCode[currentPos] != '{')
			{
				++currentPos;
			}

			const lib::StringView originalStructName = lib::StringView(&sourceCode[originalStructNameStartPos], currentPos - originalStructNameStartPos);

			SizeType structEndPos = sourceCode.find('}', currentPos);
			Uint32 braceDepth = 1u;
			while (structEndPos != lib::String::npos && braceDepth > 0u)
			{
				++structEndPos;
				if (sourceCode[structEndPos] == '{')
				{
					++braceDepth;
				}
				else if (sourceCode[structEndPos] == '}')
				{
					if (--braceDepth == 0u)
					{
						while (sourceCode[++structEndPos] != ';' && structEndPos < sourceCode.length());
					}
				}
			}

			SPT_CHECK(sourceCode[structEndPos] == ';');
			++structEndPos;

			lib::String structCode = sourceCode.substr(structTokenPos, structEndPos - structTokenPos);

			overrides[originalStructName] = { overrideStructName, std::move(structCode) };

			sourceCode.replace(structTokenPos, structEndPos - structTokenPos, ""); // Remove entire struct override definition from source code
		}
		else
		{
			break;
		}
	}

	return overrides;
}


static void ApplyTypeOverrides(lib::String& structCode, SizeType startPos, const TypeOverrideMap& overrides)
{
	SPT_PROFILER_FUNCTION();

	for (const auto& [originalType, overrideInfo] : overrides)
	{
		SizeType currentPos = startPos;

		while (true)
		{
			SizeType typePos = structCode.find(originalType.GetView(), currentPos);
			if (typePos != lib::String::npos)
			{
				const char charBefore = (typePos > 0u) ? structCode[typePos - 1u] : ' ';
				const char charAfter = (typePos + originalType.GetView().length() < structCode.length()) ? structCode[typePos + originalType.GetView().length()] : ' ';

				const auto isValidChar = [](char c) -> bool
				{
					return lib::StringUtils::IsWhiteChar(c) || c == ';' || c == '{' || c == '}' || c == '<' || c == '>';
				};

				if (isValidChar(charBefore) && isValidChar(charAfter))
				{
					structCode.replace(typePos, originalType.GetView().length(), overrideInfo.typeName.GetView());
					currentPos = typePos + overrideInfo.typeName.GetView().length();
				}
				else
				{
					currentPos = typePos + originalType.GetView().length();
				}
			}
			else
			{
				break;
			}
		}
	}
}

} // helper

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderMetaDataPrerpocessor ====================================================================

ShaderPreprocessingMetaData ShaderMetaDataPrerpocessor::PreprocessMainShaderFile(const lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	ShaderPreprocessingMetaData metaData;

	PreprocessShaderMetaParameters(sourceCode, INOUT metaData);

	return metaData;
}

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

	ShaderPreprocessingState preprocessingState;
	preprocessingState.overrides = helper::ParseTypeOverrides(sourceCode);

	PreprocessShaderDescriptorSets(sourceCode, preprocessingState, OUT metaData);
	PreprocessShaderStructs(sourceCode, preprocessingState, OUT metaData);

	RemoveMetaParameters(sourceCode);

#if WITH_SHADERS_HOT_RELOAD
	PreprocessFileDependencies(sourceCode, OUT metaData);
#endif // WITH_SHADERS_HOT_RELOAD

#if SPT_SHADERS_DEBUG_FEATURES
	PreprocessShaderLiterals(sourceCode, OUT metaData);
#endif // SPT_SHADERS_DEBUG_FEATURES

	return metaData;
}

void ShaderMetaDataPrerpocessor::PreprocessShaderMetaParameters(const lib::String& sourceCode, ShaderPreprocessingMetaData& outMetaData)
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

void ShaderMetaDataPrerpocessor::RemoveMetaParameters(lib::String& sourceCode)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex metaDataRegex(R"~(\[\[meta\((.*?)\)\]\])~");
	sourceCode = std::regex_replace(sourceCode, metaDataRegex, "");
}

void ShaderMetaDataPrerpocessor::PreprocessShaderStructs(lib::String& sourceCode, const ShaderPreprocessingState& preprocessingState, ShaderCompilationMetaData& outMetaData)
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
			const rdr::ShaderStructMetaData& structMetaData = rdr::ShaderStructsRegistry::GetStructMetaDataChecked(structName);
			structSourceCode = structMetaData.GetHLSLSourceCode();

			const SizeType overridesStartPos = structSourceCode.find('{');
			helper::ApplyTypeOverrides(INOUT structSourceCode, overridesStartPos, preprocessingState.overrides);

			if (preprocessingState.overrides.contains(structName))
			{
				const OverrideTypeInfo& overrideInfo = preprocessingState.overrides.at(structName);
				structSourceCode += '\n' + overrideInfo.typeStr;
			}

			m_definedStructs.emplace(structName);
		}

		sourceCode.replace(shaderStructsIt->prefix().length(), shaderStructMatch.length(), structSourceCode);

		// Always search for new struct from the beginning, because we're modifying source code during loop
		shaderStructsIt = std::sregex_iterator(std::cbegin(sourceCode), std::cend(sourceCode), shaderStructRegex);
	}
}

void ShaderMetaDataPrerpocessor::PreprocessShaderDescriptorSets(lib::String& sourceCode, const ShaderPreprocessingState& preprocessingState, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	lib::String accessorsCode;
	
	helper::IterateDescriptorSets([&sourceCode, &outMetaData, &accessorsCode, &preprocessingState](const lib::HashedString& dsName, Uint32 dsIdx, SizeType dsTokenPosition, SizeType dsTokenLength)
								  {
									  const DescriptorSetCompilationDef& dsCompilationDef = DescriptorSetCompilationDefsRegistry::GetDescriptorSetCompilationDef(dsName);

									  lib::String dsSourceCode = dsCompilationDef.GetShaderCode(dsIdx);
									  helper::ApplyTypeOverrides(INOUT dsSourceCode, 0u, preprocessingState.overrides);

									  sourceCode.replace(dsTokenPosition, dsTokenLength, dsSourceCode);

									  accessorsCode += dsCompilationDef.GetAccessorsCode();

									  outMetaData.AddDescriptorSetMetaData(dsIdx, dsCompilationDef.GetMetaData());
									  
									  return helper::EDSIteratorFuncResult::ContinueFromStart;
								  },
								  sourceCode);

	if (!accessorsCode.empty())
	{
		sourceCode.insert(sourceCode.begin(), accessorsCode.cbegin(), accessorsCode.cend());
	}
}

#if WITH_SHADERS_HOT_RELOAD
void ShaderMetaDataPrerpocessor::PreprocessFileDependencies(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData)
{
	SPT_PROFILER_FUNCTION();

	static const std::regex includeFileRegex(R"~(#line.*\"(.*)\")~");

	for (std::sregex_iterator it(std::cbegin(sourceCode), std::cend(sourceCode), includeFileRegex);
			it != std::sregex_iterator();
			++it)
	{
		const std::smatch includeFileMatch = *it;
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
