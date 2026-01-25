
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


struct OverrideTypeInfo
{
	lib::HashedString typeName;
	lib::String       typeStr;
};


using TypeOverrideMap = lib::HashMap<lib::HashedString, OverrideTypeInfo>;


struct ShaderPreprocessingState
{
	TypeOverrideMap overrides;
};


class ShaderMetaDataPrerpocessor
{
public:

	SPT_NODISCARD static ShaderPreprocessingMetaData PreprocessMainShaderFile(const lib::String& sourceCode);
	SPT_NODISCARD static ShaderPreprocessingMetaData PreprocessAdditionalCompilerArgs(const lib::String& sourceCode);
	SPT_NODISCARD static ShaderCompilationMetaData   PreprocessShader(lib::String& sourceCode);
};

} // spt::sc
