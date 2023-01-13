#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "Common/CompiledShader.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class CompilerImpl;
class ShaderCompilationSettings;
struct ShaderStageCompilationDef;
struct ShaderParametersMetaData;


class SHADER_COMPILER_API ShaderCompiler
{
public:

	ShaderCompiler();
	~ShaderCompiler();

	CompiledShader CompileShader(const lib::String& shaderPath, const lib::String& sourceCode, const ShaderStageCompilationDef& stageCompilationDef, const ShaderCompilationSettings& compilationSettings, ShaderParametersMetaData& outParamsMetaData) const;

private:

	lib::UniquePtr<CompilerImpl> m_impl;
};

}