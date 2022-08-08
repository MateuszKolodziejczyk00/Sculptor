#include "ShaderCompilationInput.h"

namespace spt::sc
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderSourceCode ==============================================================================

ShaderSourceCode::ShaderSourceCode()
{ }

void ShaderSourceCode::SetSourceCode(lib::String&& code)
{
	m_code = std::move(code);
}

const lib::String& ShaderSourceCode::GetSourceCode() const
{
	return m_code;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompilationSettings =====================================================================

ShaderCompilationSettings::ShaderCompilationSettings()
	: m_type(ECompiledShaderType::None)
{ }

void ShaderCompilationSettings::SetShaderType(ECompiledShaderType type)
{
	m_type = type;
}

void ShaderCompilationSettings::AddMacroDefinition(MacroDefinition macro)
{
	SPT_PROFILE_FUNCTION();

	m_macros.push_back(macro);
}

const lib::DynamicArray<MacroDefinition>& ShaderCompilationSettings::GetMacros() const
{
	return m_macros;
}

ECompiledShaderType ShaderCompilationSettings::GetType() const
{
	return m_type;
}

}
