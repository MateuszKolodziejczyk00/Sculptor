#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

struct ShaderStageCompilationDef;


class CompilationErrorsLogger
{
public:

	static void				OutputShaderPreprocessedCode(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef);

	static void				OutputShaderPreprocessingErrors(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const lib::String& errors);

	static void				OutputShaderCompilationErrors(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const lib::String& errors);

private:

	static lib::String		GetShaderLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef);

	static lib::String		GetShaderPreprocessedCodeLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef);
	static lib::String		GetShaderPreprocesingErrorsLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef);
	static lib::String		GetShaderCompilationErrorsLogsPath(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef);

};

} // spt::sc