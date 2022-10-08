#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "DescriptorSetCompilationDefTypes.h"


namespace spt::sc
{

class SHADER_COMPILER_API DescriptorSetCompilationDefRegistration
{
public:

	DescriptorSetCompilationDefRegistration(const lib::HashedString& dsName, const lib::String& dsCode, const DescriptorSetCompilationMetaData& metaData);
};

} // spt::sc
