
#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataBuilderTypes.h"

namespace spt::sc
{

struct ShaderPreprocessingMetaData
{
	lib::DynamicArray<lib::HashedString> macroDefinitions;

	Bool forceDebugMode = false;
};


class ShaderMetaDataPrerpocessor
{
public:

	SPT_NODISCARD static ShaderPreprocessingMetaData PreprocessMainShaderFile(const lib::String& sourceCode);
	SPT_NODISCARD static ShaderPreprocessingMetaData PreprocessAdditionalCompilerArgs(const lib::String& sourceCode);
	SPT_NODISCARD static ShaderCompilationMetaData   PreprocessShader(lib::String& sourceCode);

private:
	
	static void PreprocessShaderMetaParameters(const lib::String& sourceCode, ShaderPreprocessingMetaData& outMetaData);

	static void RemoveMetaParameters(lib::String& sourceCode);

	static void PreprocessShaderStructs(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData);
	static void PreprocessShaderDescriptorSets(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData);

#if SPT_SHADERS_DEBUG_FEATURES
	static void PreprocessShaderLiterals(lib::String& sourceCode, ShaderCompilationMetaData& outMetaData);
#endif // SPT_SHADERS_DEBUG_FEATURES
};

} // spt::sc
