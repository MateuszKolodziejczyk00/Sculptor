#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "Common/CompiledShader.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class CompilerImpl;
class ShaderSourceCode;
class ShaderCompilationSettings;


class SHADER_COMPILER_API ShaderCompiler
{
public:

	ShaderCompiler();
	~ShaderCompiler();

	CompiledShader					CompileShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings) const;

private:

	lib::UniquePtr<CompilerImpl>	m_impl;
};

}