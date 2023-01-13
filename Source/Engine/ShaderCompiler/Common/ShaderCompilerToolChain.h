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

	static Bool CompileShader(const lib::String& shaderRelativePath, const ShaderCompilationSettings& settings, EShaderCompilationFlags compilationFlags, CompiledShaderFile& outCompiledShaders); 

private:

	static Bool CompilePreprocessedShaders(const lib::String& shaderRelativePath, const lib::String& shaderCode, const ShaderCompilationSettings& settings, const ShaderCompiler& compiler, CompiledShaderFile& outCompiledShaders);
};

} // spt::sc
