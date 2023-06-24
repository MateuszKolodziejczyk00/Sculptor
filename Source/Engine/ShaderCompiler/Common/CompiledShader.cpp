#include "CompiledShader.h"

namespace spt::sc
{

CompiledShader::CompiledShader()
	: stage(rhi::EShaderStage::None)
{ }

Bool CompiledShader::IsValid() const
{
	return stage != rhi::EShaderStage::None && !binary.empty() && entryPoint.IsValid();
}

} // spt::sc
