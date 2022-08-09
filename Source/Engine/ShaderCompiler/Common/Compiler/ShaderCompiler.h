#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class CompilerImpl;
class ShaderSourceCode;
class ShaderCompilationSettings;


struct ShaderCompilationResult
{
public:

	ShaderCompilationResult() = default;

	Bool IsSuccess() const
	{
		return !m_binary.empty();
	}

	lib::DynamicArray<Uint32>		m_binary;
};


class SHADER_COMPILER_API ShaderCompiler
{
public:

	ShaderCompiler();

	ShaderCompilationResult				CompileShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

private:

	lib::UniquePtr<CompilerImpl>		m_impl;
};

}