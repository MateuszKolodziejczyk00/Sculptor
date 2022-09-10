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


struct DescriptorSetBindingDefinition
{
	DescriptorSetBindingDefinition()
		: bindingIdx(idxNone<Uint32>)
		, descriptorType(EDescriptorType::None)
		, descriptorCount(0)
		, shaderStages(EShaderStageFlags::None)
		, flags(EDescriptorSetBindingFlags::None)
	{ }

	Uint32							bindingIdx;
	EDescriptorType					descriptorType;
	Uint32							descriptorCount;
	EShaderStageFlags				shaderStages;
	EDescriptorSetBindingFlags		flags;
};


enum class EDescriptorSetFlags
{
	None						= 0
};


struct DescriptorSetDefinition
{
	DescriptorSetDefinition()
		: flags(EDescriptorSetFlags::None)
	{ }

	lib::DynamicArray<DescriptorSetBindingDefinition>	bindings;
	EDescriptorSetFlags									flags;
};

using DescriptorSetLayoutID = SizeType;

} // spt::rhi


namespace std
{

template<>
struct hash<spt::rhi::DescriptorSetBindingDefinition>
{
    size_t operator()(const spt::rhi::DescriptorSetBindingDefinition& binidngDef) const
    {
		size_t seed = 0;
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.bindingIdx));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.descriptorType));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.descriptorCount));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.shaderStages));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.flags));
		return seed;
    }
};


template<>
struct hash<spt::rhi::DescriptorSetDefinition>
{
    size_t operator()(const spt::rhi::DescriptorSetDefinition& dsDef) const
    {
		size_t seed = 0;
		seed = spt::lib::HashRange(std::cbegin(dsDef.bindings), std::cend(dsDef.bindings));
		spt::lib::HashCombine(seed, static_cast<size_t>(dsDef.flags));
		return seed;
    }
};

} // std
