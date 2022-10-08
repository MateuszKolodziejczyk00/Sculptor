#pragma once

#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::sc
{

struct ShaderFilePreprocessingResult
{
	ShaderFilePreprocessingResult() = default;

	Bool IsValid() const;

	lib::DynamicArray<ShaderSourceCode>		shaders;
};

class ShaderFilePreprocessor
{
public:

	static ShaderFilePreprocessingResult PreprocessShaderFileSourceCode(const lib::String& sourceCode);
};

} // spt::sc