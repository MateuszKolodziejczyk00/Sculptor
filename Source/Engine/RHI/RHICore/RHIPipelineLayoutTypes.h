#pragma once

#include "SculptorCoreTypes.h"
#include "RHIDescriptorSetDefinition.h"
#include "RHIBridge/RHIDescriptorSetLayoutImpl.h"

namespace spt::rhi
{

struct PipelineLayoutDefinition
{
	PipelineLayoutDefinition() = default;

	lib::DynamicArray<RHIDescriptorSetLayout> descriptorSetLayouts;
};

} // spt::rhi

namespace std
{

template<>
struct hash<spt::rhi::PipelineLayoutDefinition>
{
    size_t operator()(const spt::rhi::PipelineLayoutDefinition& pipelineLayoutDefinition) const
    {
		const auto dsLayoutHasher = [](const spt::rhi::RHIDescriptorSetLayout& layout)
		{
			return layout.GetHash();
		};

		size_t seed = 0;
		seed = spt::lib::HashRange(std::cbegin(pipelineLayoutDefinition.descriptorSetLayouts), std::cend(pipelineLayoutDefinition.descriptorSetLayouts), dsLayoutHasher);
		return seed;
    }
};

} // std
