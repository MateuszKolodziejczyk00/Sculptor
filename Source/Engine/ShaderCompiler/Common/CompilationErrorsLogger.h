#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class ShaderSourceCode;


class CompilationErrorsLogger
{
public:

	static void				OutputShaderPreprocessedCodeCode(const ShaderSourceCode& sourceCode);

private:

	static lib::String		GetShaderLogsPath(const ShaderSourceCode& sourceCode);

	static lib::String		GetShaderPreprocessedCodeLogsPath(const ShaderSourceCode& sourceCode);

};

}