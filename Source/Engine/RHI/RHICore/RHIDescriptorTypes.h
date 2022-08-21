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
		: m_bindingIdx(idxNone<Uint32>)
		, m_descriptorType(EDescriptorType::None)
		, m_descriptorCount(0)
		, m_shaderStages(EShaderStageFlags::None)
		, m_flags(EDescriptorSetBindingFlags::None)
	{ }

	Uint32							m_bindingIdx;
	EDescriptorType					m_descriptorType;
	Uint32							m_descriptorCount;
	EShaderStageFlags				m_shaderStages;
	EDescriptorSetBindingFlags		m_flags;
};


enum class EDescriptorSetFlags
{
	None						= 0
};


struct DescriptorSetDefinition
{
	DescriptorSetDefinition()
		: m_flags(EDescriptorSetFlags::None)
	{ }

	lib::DynamicArray<DescriptorSetBindingDefinition>	m_bindings;
	EDescriptorSetFlags									m_flags;
};


} // spt::rhi


namespace std
{

template<>
struct hash<spt::rhi::DescriptorSetBindingDefinition>
{
    size_t operator()(const spt::rhi::DescriptorSetBindingDefinition& binidngDef) const
    {
		size_t seed = 0;
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.m_bindingIdx));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.m_descriptorType));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.m_descriptorCount));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.m_shaderStages));
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.m_flags));
		return seed;
    }
};


template<>
struct hash<spt::rhi::DescriptorSetDefinition>
{
    size_t operator()(const spt::rhi::DescriptorSetDefinition& dsDef) const
    {
		size_t seed = 0;
		seed = spt::lib::HashRange(std::cbegin(dsDef.m_bindings), std::cend(dsDef.m_bindings));
		spt::lib::HashCombine(seed, static_cast<size_t>(dsDef.m_flags));
		return seed;
    }
};

} // std
