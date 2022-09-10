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
	RHIDescriptorSet(VkDescriptorSet handle, Uint16 poolSetIdx, Uint16 poolIdx);

	Bool IsValid() const;

	// Vulkan ================================================

	VkDescriptorSet		GetHandle() const;
	Uint16				GetPoolSetIdx() const;
	Uint16				GetPoolIdx() const;

private:

	VkDescriptorSet		m_handle;
	Uint16				m_poolSetIdx;
	Uint16				m_poolIdx;
};

} // spt::vulkan