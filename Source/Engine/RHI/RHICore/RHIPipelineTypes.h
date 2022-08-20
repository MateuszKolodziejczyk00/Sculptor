#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rhi
{

enum class EPipelineStage : Flags64
{
	None							= 0,
	TopOfPipe						= BIT64(0),
	DrawIndirect					= BIT64(1),
	VertexShader					= BIT64(2),
	FragmentShader					= BIT64(3),
	EarlyFragmentTest				= BIT64(4),
	LateFragmentTest				= BIT64(5),
	ColorRTOutput					= BIT64(6),
	ComputeShader					= BIT64(7),
	Transfer						= BIT64(8),
	BottomOfPipe					= BIT64(9),
	Copy							= BIT64(10),
	Blit							= BIT64(11),
	Clear							= BIT64(12),
	IndexInput						= BIT64(13),

	ALL_GRAPHICS					= BIT64(50),
	ALL_TRANSFER					= BIT64(51),
	ALL_COMMANDS					= BIT64(52)
};


enum class EDescriptorType
{
	None,
	Texture,
	CombinedTextureSampler,
	StorageBuffer,
	StorageBufferDynamicOffset
};


enum class EBindingFlags
{
	None							= 0
};


struct DescriptorSetBindingDefinition
{
	DescriptorSetBindingDefinition()
		: m_bindingIdx(idxNone<Uint32>)
		, m_descriptorType(EDescriptorType::None)
		, m_descriptorCount(0)
		, m_pipelineStages(EPipelineStage::None)
		, m_flags(EBindingFlags::None)
	{ }

	Uint32				m_bindingIdx;
	EDescriptorType		m_descriptorType;
	Uint32				m_descriptorCount;
	EPipelineStage		m_pipelineStages;
	EBindingFlags		m_flags;
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


struct PipelineLayoutDefinition
{
	PipelineLayoutDefinition() = default;

	lib::DynamicArray<DescriptorSetDefinition>		m_descriptorSets;
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
		spt::lib::HashCombine(seed, static_cast<size_t>(binidngDef.m_pipelineStages));
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


template<>
struct hash<spt::rhi::PipelineLayoutDefinition>
{
    size_t operator()(const spt::rhi::PipelineLayoutDefinition& pipelineLayoutDefinition) const
    {
		size_t seed = 0;
		seed = spt::lib::HashRange(std::cbegin(pipelineLayoutDefinition.m_descriptorSets), std::cend(pipelineLayoutDefinition.m_descriptorSets));
		return seed;
    }
};

} // std
