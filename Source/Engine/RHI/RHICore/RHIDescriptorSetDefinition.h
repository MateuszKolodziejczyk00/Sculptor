#pragma once

#include "SculptorCoreTypes.h"
#include "RHIDescriptorTypes.h"
#include "RHIBridge/RHISamplerImpl.h"


namespace spt::rhi
{

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
	RHISampler						immutableSampler;
};


struct DescriptorSetDefinition
{
	DescriptorSetDefinition()
		: flags(EDescriptorSetFlags::None)
	{ }

	lib::DynamicArray<DescriptorSetBindingDefinition>	bindings;
	EDescriptorSetFlags									flags;
};

} // spt::rhi


namespace std
{

template<>
struct hash<spt::rhi::DescriptorSetBindingDefinition>
{
	size_t operator()(const spt::rhi::DescriptorSetBindingDefinition& binidngDef) const
	{
		return spt::lib::HashCombine(binidngDef.bindingIdx,
									 binidngDef.descriptorType,
									 binidngDef.descriptorCount,
									 binidngDef.shaderStages,
									 binidngDef.flags,
									 binidngDef.immutableSampler.GetHandle());
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
