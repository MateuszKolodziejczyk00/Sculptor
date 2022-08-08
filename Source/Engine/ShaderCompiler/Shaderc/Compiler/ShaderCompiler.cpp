#include "Common/Compiler/ShaderCompiler.h"
#include "shaderc/shaderc.hpp"

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompilerImpl ==================================================================================

class CompilerImpl
{
public:

	CompilerImpl();

private:

	shaderc::Compiler	m_compiler;
};

CompilerImpl::CompilerImpl()
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompiler ================================================================================

ShaderCompiler::ShaderCompiler()
{ }

}
