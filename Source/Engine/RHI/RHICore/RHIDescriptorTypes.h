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
	CBV_SRV_UAV,
	Num,

	ResourceBegin = CombinedTextureSampler,
	ResourceEnd   = Num,
};


enum class EDescriptorSetBindingFlags : Flags32
{
	None					= 0,
	PartiallyBound			= BIT(1)
};


enum class EDescriptorSetFlags
{
	None = 0
};


struct DescriptorProps
{
	Uint32 resourceDescriptorSize = 0u;
	Uint32 samplerDescriptorSize  = 0u;

	Uint32 bufferDescriptorIdxFactor = 1u;
	Uint32 textureDescriptorIdxFactor = 1u;

	Uint32 reservedResourceHeapSize = 0u;
	Uint32 reservedSamplerHeapSize  = 0u;

	Uint32 maxResourceHeapSize = 0u;
	Uint32 maxSamplerHeapSize  = 0u;

	Uint32 descriptorsAlignment = 0u;
};


enum class EDescriptorHeapType
{
	Resource,
	Sampler,
};


struct DescriptorHeapDefinition
{
	Uint64              size = 0u;
	EDescriptorHeapType type = EDescriptorHeapType::Resource;
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
