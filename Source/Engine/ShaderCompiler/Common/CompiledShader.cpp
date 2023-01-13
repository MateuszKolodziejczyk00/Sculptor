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
	return m_stage != rhi::EShaderStage::None && !m_binary.empty() && m_entryPoint.IsValid();
}

void CompiledShader::SetBinary(Binary binary)
{
	m_binary = std::move(binary);
}

void CompiledShader::SetStage(rhi::EShaderStage stage)
{
	m_stage = stage;
}

void CompiledShader::SetEntryPoint(const lib::HashedString& entryPoint)
{
	m_entryPoint = entryPoint;
}

const CompiledShader::Binary& CompiledShader::GetBinary() const
{
	return m_binary;
}

rhi::EShaderStage CompiledShader::GetStage() const
{
	return m_stage;
}

const lib::HashedString& CompiledShader::GetEntryPoint() const
{
	return m_entryPoint;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CompiledShaderFile ============================================================================

CompiledShaderFile::CompiledShaderFile()
{ }

Bool CompiledShaderFile::IsValid() const
{
	return !shaders.empty()
		&& std::all_of(std::cbegin(shaders), std::cend(shaders), [](const CompiledShader& shader) { return shader.IsValid(); });
}

} // spt::sc
