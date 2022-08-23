#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class ShaderSourceCode;


class CompilationErrorsLogger
{
public:

	static void				OutputShaderPreprocessedCode(const lib::String& shaderPath, const ShaderSourceCode& sourceCode);

	static void				OutputShaderPreprocessingErrors(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const lib::String& errors);

	static void				OutputShaderCompilationErrors(const lib::String& shaderPath, const ShaderSourceCode& sourceCode, const lib::String& errors);

private:

	static lib::String		GetShaderLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode);

	static lib::String		GetShaderPreprocessedCodeLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode);
	static lib::String		GetShaderPreprocesingErrorsLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode);
	static lib::String		GetShaderCompilationErrorsLogsPath(const lib::String& shaderPath, const ShaderSourceCode& sourceCode);

};

}