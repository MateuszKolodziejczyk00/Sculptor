#pragma once

#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::sc
{

struct PreprocessedShadersGroup
{
	explicit PreprocessedShadersGroup(lib::HashedString inGroupName);

	Bool									IsValid() const;

	lib::HashedString						groupName;
	lib::DynamicArray<ShaderSourceCode>		shaders;
};


struct ShaderFilePreprocessingResult
{
	ShaderFilePreprocessingResult() = default;

	Bool											IsValid() const;

	lib::DynamicArray<PreprocessedShadersGroup>		shadersGroups;
};


class ShaderFilePreprocessor
{
public:

	static ShaderFilePreprocessingResult		PreprocessShaderFileSourceCode(const lib::String& sourceCode);
	
};

}