#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"
#include "RHI/RHICore/RHISamplerTypes.h"


namespace spt::sc
{

struct DescriptorSetCompilationMetaData
{
	DescriptorSetCompilationMetaData()
		: typeID(idxNone<SizeType>)
	{ }

	lib::DynamicArray<lib::HashedString> additionalMacros;

	lib::HashMap<Uint32, rhi::SamplerDefinition> bindingToImmutableSampler;

	SizeType typeID;
};

} // spt::sc