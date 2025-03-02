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

namespace spt::lib
{

template<>
struct Hasher<rhi::PipelineLayoutDefinition>
{
    size_t operator()(const rhi::PipelineLayoutDefinition& pipelineLayoutDefinition) const
    {
		const auto dsLayoutHasher = [](const rhi::RHIDescriptorSetLayout& layout)
		{
			return layout.GetHash();
		};

		size_t seed = 0;
		seed = HashRange(std::cbegin(pipelineLayoutDefinition.descriptorSetLayouts), std::cend(pipelineLayoutDefinition.descriptorSetLayouts), dsLayoutHasher);
		return seed;
    }
};

} // spt::lib
