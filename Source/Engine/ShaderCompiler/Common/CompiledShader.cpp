#include "CompiledShader.h"

namespace spt::sc
{

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

}
