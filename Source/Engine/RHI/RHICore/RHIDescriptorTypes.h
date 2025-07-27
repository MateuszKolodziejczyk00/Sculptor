#pragma once

#include "SculptorCoreTypes.h"
#include "RHIShaderTypes.h"
#include "RHIAllocationTypes.h"


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
	Num
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


struct DescriptorProps
{
	static constexpr Uint32 s_descriptorTypesNum = static_cast<Uint32>(EDescriptorType::Num);

	Uint32 StrideFor(EDescriptorType type) const
	{
		return strides[static_cast<Uint32>(type)];
	}

	lib::StaticArray<Uint32, s_descriptorTypesNum> strides = {};

	Uint32 descriptorsAlignment = 0u;
};


struct DescriptorHeapDefinition
{
	Uint64 size = 0u;
};


struct RHIDescriptorRange
{
	lib::Span<Byte> data = {};
	RHIVirtualAllocationHandle allocationHandle{}; // might be empty if it's part of a larger, shared allocation
	Uint32 heapOffset{};

	Bool IsValid() const
	{
		return !data.empty();
	}
};

} // spt::rhi
