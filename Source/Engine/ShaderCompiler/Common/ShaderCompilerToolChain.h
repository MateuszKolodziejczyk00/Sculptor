#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "CompiledShader.h"
#include "ShaderCompilationInput.h"


namespace spt::sc
{

class ShaderCompiler;


class SHADER_COMPILER_API ShaderCompilerToolChain
{
public:

	static CompiledShader CompileShader(const lib::String& shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings, EShaderCompilationFlags compilationFlags); 

private:

	static CompiledShader CompilePreprocessedShaders(const lib::String& shaderRelativePath, const lib::String& shaderCode, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings, const ShaderCompiler& compiler);
};

} // spt::sc
