#include "CompiledShader.h"

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShader ================================================================================

CompiledShader::CompiledShader()
	: m_type(ECompiledShaderType::None)
{ }

Bool CompiledShader::IsValid() const
{
	return m_type != ECompiledShaderType::None && !m_binary.empty();
}

void CompiledShader::SetBinary(Binary binary)
{
	m_binary = std::move(binary);
}

void CompiledShader::SetType(ECompiledShaderType type)
{
	m_type = type;
}

const CompiledShader::Binary& CompiledShader::GetBinary() const
{
	return m_binary;
}

ECompiledShaderType CompiledShader::GetType() const
{
	return m_type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShadersGroup ==========================================================================

CompiledShadersGroup::CompiledShadersGroup()
{ }

Bool CompiledShadersGroup::IsValid() const
{
	return m_groupName.IsValid()
		&& !m_shaders.empty()
		&& std::all_of(std::cbegin(m_shaders), std::cend(m_shaders), [](const CompiledShader& shader) { return shader.IsValid(); });
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShaderFile ============================================================================

CompiledShaderFile::CompiledShaderFile()
{ }

Bool CompiledShaderFile::IsValid() const
{
	return !m_groups.empty()
		&& std::all_of(std::cbegin(m_groups), std::cend(m_groups), [](const CompiledShadersGroup& group) { return group.IsValid(); });
}

}
