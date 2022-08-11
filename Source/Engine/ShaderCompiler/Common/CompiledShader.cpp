#include "CompiledShader.h"

namespace spt::sc
{

CompiledShader::CompiledShader()
{ }

Bool CompiledShader::IsValid() const
{
	return !m_binary.empty();
}

void CompiledShader::SetBinary(Binary binary)
{
	m_binary = std::move(binary);
}

const CompiledShader::Binary& CompiledShader::GetBinary() const
{
	return m_binary;
}

}
