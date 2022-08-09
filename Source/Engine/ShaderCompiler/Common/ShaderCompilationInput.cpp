#include "ShaderCompilationInput.h"

namespace spt::sc
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderSourceCode ==============================================================================

ShaderSourceCode::ShaderSourceCode(lib::HashedString name)
	: m_name(name)
	, m_type(ECompiledShaderType::None)
{
	SPT_CHECK(m_name.IsValid());
}

void ShaderSourceCode::SetSourceCode(lib::String&& code)
{
	m_code = std::move(code);
}

void ShaderSourceCode::SetShaderType(ECompiledShaderType type)
{
	m_type = type;
}

lib::HashedString ShaderSourceCode::GetName() const
{
	return m_name;
}

const lib::String& ShaderSourceCode::GetSourceCode() const
{
	return m_code;
}

ECompiledShaderType ShaderSourceCode::GetType() const
{
	return m_type;
}

const char* ShaderSourceCode::GetSourcePtr() const
{
	return m_code.data();
}

SizeType ShaderSourceCode::GetSourceLength() const
{
	return m_code.size();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompilationSettings =====================================================================

ShaderCompilationSettings::ShaderCompilationSettings()
{ }

void ShaderCompilationSettings::AddMacroDefinition(MacroDefinition macro)
{
	SPT_PROFILE_FUNCTION();

	m_macros.push_back(macro);
}

const lib::DynamicArray<MacroDefinition>& ShaderCompilationSettings::GetMacros() const
{
	return m_macros;
}

}
