#pragma once

#include "SculptorCoreTypes.h"
#include "RHIShaderTypes.h"


namespace spt::rhi
{

enum class EDescriptorType
{
	None,
	Sampler,
	CombinedTextureSampler,
	SampledTexture,
	StorageTexture,
	UniformTexelBuffer,
	StorageTexelBuffer,
	UniformBuffer,
	StorageBuffer,
	UniformBufferDynamicOffset,
	StorageBufferDynamicOffset,
	AccelerationStructure,
};


enum class EDescriptorSetBindingFlags : Flags32
{
	None					= 0,
	UpdateAfterBind			= BIT(0),
	PartiallyBound			= BIT(1)
};


enum class EDescriptorSetFlags
{
	None						= 0
};


struct WriteDescriptorDefinition
{
	WriteDescriptorDefinition()
		: bindingIdx(idxNone<Uint32>)
		, arrayElement(idxNone<Uint32>)
		, descriptorType(EDescriptorType::None)
	{ }

	Uint32			bindingIdx;
	Uint32			arrayElement;
	EDescriptorType	descriptorType;
};

} // spt::rhi
