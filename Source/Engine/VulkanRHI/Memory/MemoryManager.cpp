#include "MemoryManager.h"

namespace spt::vulkan
{

namespace priv
{

VmaVulkanFunctions BuildVulkanFunctionsPtrsTable()
{
	VmaVulkanFunctions functionsTable;
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
	functionsTable.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
	functionsTable.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
	functionsTable.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
	functionsTable.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
	functionsTable.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;

	return functionsTable;
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

}
