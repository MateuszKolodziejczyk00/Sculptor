#include "CompiledShader.h"

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShader ================================================================================

CompiledShader::CompiledShader()
	: m_stage(rhi::EShaderStage::None)
{ }

Bool CompiledShader::IsValid() const
{
	return m_stage != rhi::EShaderStage::None && !m_binary.empty();
}

void CompiledShader::SetBinary(Binary binary)
{
	m_binary = std::move(binary);
}

void CompiledShader::SetStage(rhi::EShaderStage stage)
{
	m_stage = stage;
}

const CompiledShader::Binary& CompiledShader::GetBinary() const
{
	return m_binary;
}

rhi::EShaderStage CompiledShader::GetStage() const
{
	return m_stage;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShadersGroup ==========================================================================

CompiledShadersGroup::CompiledShadersGroup()
{ }

Bool CompiledShadersGroup::IsValid() const
{
	return groupName.IsValid()
		&& !shaders.empty()
		&& std::all_of(std::cbegin(shaders), std::cend(shaders), [](const CompiledShader& shader) { return shader.IsValid(); });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShaderFile ============================================================================

CompiledShaderFile::CompiledShaderFile()
{ }

Bool CompiledShaderFile::IsValid() const
{
	return !groups.empty()
		&& std::all_of(std::cbegin(groups), std::cend(groups), [](const CompiledShadersGroup& group) { return group.IsValid(); });
}

}
