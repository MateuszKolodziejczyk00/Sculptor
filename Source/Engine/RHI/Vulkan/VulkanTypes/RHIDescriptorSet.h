#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

/**
 * Descriptor sets are not initialized as other RHI objects.
 * Instead they has to be allocated and freed using RHIDescriptorSetManager
 * This allows batching all allocations, and also makes making descriptors pools threadsafe easier.
 */
class RHI_API RHIDescriptorSet
{
public:

	RHIDescriptorSet();
	RHIDescriptorSet(VkDescriptorSet handle, VkDescriptorSetLayout layout);

	Bool IsValid() const;

	// Vulkan ================================================

	VkDescriptorSet			GetHandle() const;
	VkDescriptorSetLayout	GetLayoutHandle() const;

private:

	VkDescriptorSet			m_handle;
	VkDescriptorSetLayout	m_layout;
};

} // spt::vulkan