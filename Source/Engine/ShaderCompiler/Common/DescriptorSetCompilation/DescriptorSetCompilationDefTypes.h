#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"


namespace spt::sc
{

struct DescriptorSetCompilationMetaData
{
	DescriptorSetCompilationMetaData()
		: hash(0)
	{ }

	lib::DynamicArray<smd::EBindingFlags> bindingsFlags;
	SizeType hash;
};

} // spt::sc