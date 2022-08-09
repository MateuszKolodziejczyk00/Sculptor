#pragma once

#include "Common/ShaderCompilerMacros.h"
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

	void					CompileShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

private:

	lib::UniquePtr<CompilerImpl>		m_impl;
};

}