#include "VulkanMemoryManager.h"
#include "RHICore/RHIAllocationTypes.h"

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

} // priv

VulkanMemoryManager::VulkanMemoryManager()
	: m_allocator(VK_NULL_HANDLE)
{ }

void VulkanMemoryManager::Initialize(VkInstance instance, VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocationCallbacks)
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

void VulkanMemoryManager::Destroy()
{
	SPT_CHECK(IsValid());

	vmaDestroyAllocator(m_allocator);
	m_allocator = VK_NULL_HANDLE;
}

Bool VulkanMemoryManager::IsValid() const
{
	return m_allocator != VK_NULL_HANDLE;
}

VmaAllocator VulkanMemoryManager::GetAllocatorHandle() const
{
	return m_allocator;
}

}
