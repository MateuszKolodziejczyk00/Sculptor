#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class SHADER_COMPILER_API DescriptorSetCompilationDefRegistration
{
public:

	DescriptorSetCompilationDefRegistration(const lib::HashedString& dsName, const lib::String& dsCode);
};

} // spt::sc
