#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class CompilerImpl;


class SHADER_COMPILER_API ShaderCompiler
{
public:

	ShaderCompiler();

private:

	lib::UniquePtr<CompilerImpl>		m_impl;
};

}