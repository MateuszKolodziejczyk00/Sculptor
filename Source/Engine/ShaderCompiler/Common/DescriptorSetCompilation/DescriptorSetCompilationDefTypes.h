#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"
#include "RHI/RHICore/RHISamplerTypes.h"


namespace spt::sc
{

struct DescriptorSetCompilationMetaData
{
	DescriptorSetCompilationMetaData()
		: hash(0)
	{ }

	lib::DynamicArray<smd::EBindingFlags> bindingsFlags;

	lib::HashMap<Uint32, rhi::SamplerDefinition> bindingToImmutableSampler;

	SizeType hash;
};

} // spt::sc