
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
};

} // spt::sc
