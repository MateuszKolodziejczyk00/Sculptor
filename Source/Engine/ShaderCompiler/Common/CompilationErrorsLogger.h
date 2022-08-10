#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class ShaderSourceCode;


class CompilationErrorsLogger
{
public:

	static void				OutputShaderPreprocessedCode(const ShaderSourceCode& sourceCode);

	static void				OutputShaderPreprocessingErrors(const ShaderSourceCode& sourceCode, const lib::String& errors);

	static void				OutputShaderCompilationErrors(const ShaderSourceCode& sourceCode, const lib::String& errors);

private:

	static lib::String		GetShaderLogsPath(const ShaderSourceCode& sourceCode);

	static lib::String		GetShaderPreprocessedCodeLogsPath(const ShaderSourceCode& sourceCode);
	static lib::String		GetShaderPreprocesingErrorsLogsPath(const ShaderSourceCode& sourceCode);
	static lib::String		GetShaderCompilationErrorsLogsPath(const ShaderSourceCode& sourceCode);

};

}