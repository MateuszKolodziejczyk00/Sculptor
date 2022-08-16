#pragma once

#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::sc
{

struct PreprocessedShadersGroup
{
	explicit PreprocessedShadersGroup(lib::HashedString groupName);

	Bool									IsValid() const;

	lib::HashedString						m_groupName;
	lib::DynamicArray<ShaderSourceCode>		m_shaders;
};


struct ShaderFilePreprocessingResult
{
	ShaderFilePreprocessingResult() = default;

	Bool											IsValid() const;

	lib::DynamicArray<PreprocessedShadersGroup>		m_shadersGroups;
};


class ShaderFilePreprocessor
{
public:

	static ShaderFilePreprocessingResult		PreprocessShaderFileSourceCode(const lib::String& sourceCode);
	
};

}