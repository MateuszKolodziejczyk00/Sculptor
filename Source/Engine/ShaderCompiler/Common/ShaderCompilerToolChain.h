#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "CompiledShader.h"
#include "ShaderCompilationInput.h"


namespace spt::sc
{

struct ShaderFilePreprocessingResult;
class ShaderCompiler;


class SHADER_COMPILER_API ShaderCompilerToolChain
{
public:

	static Bool CompileShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& settings, CompiledShaderFile& outCompiledShaders); 

private:

	static Bool CompilePreprocessedShaders(const ShaderFilePreprocessingResult& preprocessingResult, const ShaderCompilationSettings& settings, ShaderCompiler& compiler, CompiledShaderFile& outCompiledShaders);
};

} // spt::sc
