#include "MemoryManager.h"
#include "RHIAllocationTypes.h"

namespace spt::vulkan
{

namespace priv
{

VmaVulkanFunctions BuildVulkanFunctionsPtrsTable()
{
	VmaVulkanFunctions functionsTable{};
	functionsTable.vkAllocateMemory = vkAllocateMemory;
	functionsTable.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	functionsTable.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	functionsTable.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
	functionsTable.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
	functionsTable.vkAllocateMemory = vkAllocateMemory;
	functionsTable.vkFreeMemory = vkFreeMemory;
	functionsTable.vkMapMemory = vkMapMemory;
	functionsTable.vkUnmapMemory = vkUnmapMemory;
	functionsTable.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
	functionsTable.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
	functionsTable.vkBindBufferMemory = vkBindBufferMemory;
	functionsTable.vkBindImageMemory = vkBindImageMemory;
	functionsTable.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	functionsTable.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	functionsTable.vkCreateBuffer = vkCreateBuffer;
	functionsTable.vkDestroyBuffer = vkDestroyBuffer;
	functionsTable.vkCreateImage = vkCreateImage;
	functionsTable.vkDestroyImage = vkDestroyImage;
	functionsTable.vkCmdCopyBuffer = vkCmdCopyBuffer;
	functionsTable.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
	functionsTable.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
	functionsTable.vkBindBufferMemory2KHR = vkBindBufferMemory2;
	functionsTable.vkBindImageMemory2KHR = vkBindImageMemory2;
	functionsTable.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
	functionsTable.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
	functionsTable.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

	return functionsTable;
}

VmaMemoryUsage GetVMAMemoryUsage(rhicore::EMemoryUsage memoryUsage)
{
	switch (memoryUsage)
	{
	case rhicore::EMemoryUsage::GPUOnly:		return VMA_MEMORY_USAGE_GPU_ONLY;
	case rhicore::EMemoryUsage::CPUOnly:		return VMA_MEMORY_USAGE_CPU_ONLY;
	case rhicore::EMemoryUsage::CPUToGPU:		return VMA_MEMORY_USAGE_CPU_TO_GPU;
	case rhicore::EMemoryUsage::GPUToCpu:		return VMA_MEMORY_USAGE_GPU_TO_CPU;
	case rhicore::EMemoryUsage::CPUCopy:		return VMA_MEMORY_USAGE_CPU_COPY;
	}

	SPT_CHECK_NO_ENTRY();
	return VMA_MEMORY_USAGE_AUTO;
}

VmaAllocationCreateFlags GetVMAAllocationFlags(Flags32 flags)
{
	VmaAllocationCreateFlags vmaFlags{};

	if ((flags & rhicore::EAllocationFlags::CreateDedicatedAllocation) != 0)
	{
		vmaFlags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	}
	if ((flags & rhicore::EAllocationFlags::CreateMapped) != 0)
	{
		vmaFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}

	return vmaFlags;
}

}

MemoryManager::MemoryManager()
	: m_allocator(VK_NULL_HANDLE)
{ }

void MemoryManager::Initialize(VkInstance instance, VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocationCallbacks)
{
	const VmaVulkanFunctions vulkanFunctionsTable = priv::BuildVulkanFunctionsPtrsTable();

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = logicalDevice;
	allocatorInfo.instance = instance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctionsTable;
	allocatorInfo.pAllocationCallbacks = allocationCallbacks;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	SPT_VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator));
}

void MemoryManager::Destroy()
{
	SPT_CHECK(IsValid());

	vmaDestroyAllocator(m_allocator);
	m_allocator = VK_NULL_HANDLE;
}

Bool MemoryManager::IsValid() const
{
	return m_allocator != VK_NULL_HANDLE;
}

VmaAllocator MemoryManager::GetAllocatorHandle() const
{
	return m_allocator;
}

VmaAllocationCreateInfo MemoryManager::CreateAllocationInfo(const rhicore::RHIAllocationInfo& rhiAllocationInfo) const
{
	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags = priv::GetVMAAllocationFlags(rhiAllocationInfo.m_allocationFlags);
	allocationInfo.usage = priv::GetVMAMemoryUsage(rhiAllocationInfo.m_memoryUsage);

	return allocationInfo;
}

}
