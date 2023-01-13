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
{ }

void ShaderCompilationSettings::AddShaderToCompile(const ShaderStageCompilationDef& stageCompilationDef)
{
	m_stagesToCompile.emplace_back(stageCompilationDef);
}

const lib::DynamicArray<ShaderStageCompilationDef>& ShaderCompilationSettings::GetStagesToCompile() const
{
	return m_stagesToCompile;
}

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

SizeType ShaderCompilationSettings::Hash() const
{
	SPT_PROFILER_FUNCTION();

	return lib::HashCombine(lib::HashRange(std::cbegin(m_macros), std::cend(m_macros)),
							lib::HashRange(std::cbegin(m_stagesToCompile), std::cend(m_stagesToCompile)));
}

} // spt::sc
