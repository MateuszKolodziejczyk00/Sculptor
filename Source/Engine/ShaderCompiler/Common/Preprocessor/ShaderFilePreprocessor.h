#pragma once

#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::sc
{

struct PreprocessedShadersGroup
{
	lib::DynamicArray<ShaderSourceCode>		m_shaders;
};


struct ShaderFilePreprocessingResult
{
	lib::DynamicArray<PreprocessedShadersGroup>		m_shadersGroups;
};


class ShaderFilePreprocessor
{
public:

	static ShaderFilePreprocessingResult		PreprocessShaderFileSourceCode(const lib::String& sourceCode);
	
};

}