#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan.h"


namespace spt::vulkan
{

class DescriptorPool
{
	DescriptorPool();

	void					Initialize(VkDescriptorPoolCreateFlags flags, Uint32 maxSetsNum, const lib::DynamicArray<VkDescriptorPoolSize>& poolSizes);
	void					Release();

	Bool					IsValid() const;

	VkDescriptorPool		GetHandle() const;

private:

	VkDescriptorPool		m_poolHandle;
};

}