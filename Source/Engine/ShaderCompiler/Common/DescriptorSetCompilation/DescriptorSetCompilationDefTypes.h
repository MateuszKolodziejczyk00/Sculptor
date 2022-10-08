#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"


namespace spt::sc
{

struct DescriptorSetCompilationMetaData
{
	DescriptorSetCompilationMetaData() = default;

	lib::DynamicArray<smd::EBindingFlags> bindingsFlags;
};

} // spt::sc