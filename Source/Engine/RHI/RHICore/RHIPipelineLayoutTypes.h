#pragma once

#include "SculptorCoreTypes.h"
#include "RHIDescriptorTypes.h"

namespace spt::rhi
{

struct PipelineLayoutDefinition
{
	PipelineLayoutDefinition() = default;

	lib::DynamicArray<DescriptorSetDefinition>		m_descriptorSets;
};


} // spt::rhi

namespace std
{

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
