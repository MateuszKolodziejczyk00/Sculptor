#include "ShaderCompilationInput.h"

namespace spt::sc
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderSourceCode ==============================================================================

ShaderSourceCode::ShaderSourceCode()
	: m_stage(rhi::EShaderStage::None)
{ }

ShaderSourceCode::ShaderSourceCode(lib::String code, rhi::EShaderStage stage)
	: m_code(std::move(code))
	, m_stage(stage)
{ }

Bool ShaderSourceCode::IsValid() const
{
	return m_stage != rhi::EShaderStage::None
		&& !m_code.empty();
}

void ShaderSourceCode::SetSourceCode(lib::String&& code)
{
	m_code = std::move(code);
}

void ShaderSourceCode::SetShaderStage(rhi::EShaderStage stage)
{
	m_stage = stage;
}

const lib::String& ShaderSourceCode::GetSourceCode() const
{
	return m_code;
}

lib::String& ShaderSourceCode::GetSourceCodeMutable()
{
	return m_code;
}

rhi::EShaderStage ShaderSourceCode::GetShaderStage() const
{
	return m_stage;
}

const char* ShaderSourceCode::GetSourcePtr() const
{
	return m_code.data();
}

SizeType ShaderSourceCode::GetSourceLength() const
{
	return m_code.size();
}

SizeType ShaderSourceCode::Hash() const
{
	SPT_PROFILER_FUNCTION();

	return static_cast<SizeType>(m_stage);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderCompilationSettings =====================================================================

ShaderCompilationSettings::ShaderCompilationSettings()
	: m_generateDebugSource(true)
{ }

void ShaderCompilationSettings::AddMacroDefinition(MacroDefinition macro)
{
	SPT_PROFILER_FUNCTION();

	if (macro.IsValid())
	{
		m_macros.push_back(macro.macro);
	}
}

const lib::DynamicArray<lib::HashedString>& ShaderCompilationSettings::GetMacros() const
{
	return m_macros;
}

Bool ShaderCompilationSettings::ShouldGenerateDebugSource() const
{
	return m_generateDebugSource;
}

void ShaderCompilationSettings::DisableGeneratingDebugSource()
{
	m_generateDebugSource = false;
}

SizeType ShaderCompilationSettings::Hash() const
{
	SPT_PROFILER_FUNCTION();

	return lib::HashRange(std::cbegin(m_macros), std::cend(m_macros));
}

} // spt::sc
