#include "LogicalDevice.h"
#include "VulkanDeviceCommon.h"
#include "PhysicalDevice.h"
#include "Vulkan/VulkanUtils.h"
#include "Vulkan/VulkanRHI.h"


namespace spt::vulkan
{

LogicalDevice::LogicalDevice()
	: m_deviceHandle(VK_NULL_HANDLE)
	, m_gfxFamilyIdx(idxNone<Uint32>)
	, m_asyncComputeFamilyIdx(idxNone<Uint32>)
	, m_transferFamilyIdx(idxNone<Uint32>)
	, m_gfxQueueHandle(VK_NULL_HANDLE)
	, m_asyncComputeQueueHandle(VK_NULL_HANDLE)
	, m_transferQueueHandle(VK_NULL_HANDLE)
{ }

void LogicalDevice::CreateDevice(VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<const char*> deviceExtensions = VulkanDeviceCommon::GetRequiredDeviceExtensions();

	const lib::DynamicArray<VkDeviceQueueCreateInfo> queueInfos = CreateQueueInfos(physicalDevice);

	VkDeviceCreateInfo deviceInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.queueCreateInfoCount = static_cast<Uint32>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = static_cast<Uint32>(deviceExtensions.size());
	deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VulkanStructsLinkedList deviceInfoLinkedData(deviceInfo);

	VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	features.features.multiViewport		= VK_TRUE;
	features.features.samplerAnisotropy = VK_TRUE;
	features.features.sampleRateShading = VK_TRUE;
	features.features.independentBlend	= VK_TRUE;
	features.features.fillModeNonSolid	= VK_TRUE;
	features.features.shaderInt16		= VK_TRUE;

	deviceInfoLinkedData.Append(features);

	VkPhysicalDeviceVulkan11Features vulkan11Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    vulkan11Features.shaderDrawParameters				= VK_TRUE;
    vulkan11Features.storageBuffer16BitAccess			= VK_TRUE;

	deviceInfoLinkedData.Append(vulkan11Features);

	VkPhysicalDeviceVulkan12Features vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	vulkan12Features.bufferDeviceAddress	= VK_TRUE;
	vulkan12Features.timelineSemaphore		= VK_TRUE;
	vulkan12Features.drawIndirectCount		= VK_TRUE;
	vulkan12Features.shaderFloat16			= VK_TRUE;
	vulkan12Features.descriptorBindingSampledImageUpdateAfterBind	= VK_TRUE;
	vulkan12Features.descriptorBindingPartiallyBound				= VK_TRUE;
	

	deviceInfoLinkedData.Append(vulkan12Features);

	VkPhysicalDeviceVulkan13Features vulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	vulkan13Features.synchronization2 = VK_TRUE;
	vulkan13Features.dynamicRendering = VK_TRUE;

	deviceInfoLinkedData.Append(vulkan13Features);

#if WITH_NSIGHT_AFTERMATH
	VkDeviceDiagnosticsConfigCreateInfoNV aftermathInfo{ VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV };

	if (VulkanRHI::GetSettings().AreGPUCrashDumpsEnabled())
	{
		const VkDeviceDiagnosticsConfigFlagsNV aftermathFlags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV
															  | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV
															  | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV;
		aftermathInfo.flags = aftermathFlags;
		
		deviceInfoLinkedData.Append(aftermathInfo);
	}
#endif // WITH_NSIGHT_AFTERMATH

	SPT_VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, allocator, &m_deviceHandle));

	VkDeviceQueueInfo2 gfxQueueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
    gfxQueueInfo.queueFamilyIndex = m_gfxFamilyIdx;
    gfxQueueInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_deviceHandle, &gfxQueueInfo, &m_gfxQueueHandle);

	VkDeviceQueueInfo2 asyncComputeQueueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
    asyncComputeQueueInfo.queueFamilyIndex = m_asyncComputeFamilyIdx;
    asyncComputeQueueInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_deviceHandle, &asyncComputeQueueInfo, &m_asyncComputeQueueHandle);

	VkDeviceQueueInfo2 transferQueueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
    transferQueueInfo.queueFamilyIndex = m_transferFamilyIdx;
    transferQueueInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_deviceHandle, &transferQueueInfo, &m_transferQueueHandle);

	volkLoadDevice(m_deviceHandle);
}

void LogicalDevice::Destroy(const VkAllocationCallbacks* allocator)
{
	SPT_CHECK(IsValid());

	vkDestroyDevice(m_deviceHandle, allocator);
	
	m_deviceHandle = VK_NULL_HANDLE;
}

VkDevice LogicalDevice::GetHandle() const
{
	return m_deviceHandle;
}

void LogicalDevice::WaitIdle() const
{
	vkDeviceWaitIdle(m_deviceHandle);
}

bool LogicalDevice::IsValid() const
{
	return m_deviceHandle != VK_NULL_HANDLE;
}

lib::DynamicArray<VkDeviceQueueCreateInfo> LogicalDevice::CreateQueueInfos(VkPhysicalDevice physicalDevice)
{
	m_gfxFamilyIdx = idxNone<Uint32>;
	m_asyncComputeFamilyIdx = idxNone<Uint32>;
	m_transferFamilyIdx = idxNone<Uint32>;

	const lib::DynamicArray<VkQueueFamilyProperties2> queueFamiliies = PhysicalDevice::GetDeviceQueueFamilies(physicalDevice);

	for (size_t familyIdx = 0; familyIdx < queueFamiliies.size(); ++familyIdx)
	{
		const VkQueueFamilyProperties2& queueFamilyProps = queueFamiliies[familyIdx];
		const VkQueueFlags queueFlags = queueFamilyProps.queueFamilyProperties.queueFlags;

		if (m_gfxFamilyIdx == idxNone<Uint32>)
		{
			if ((queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			{
				m_gfxFamilyIdx = static_cast<Uint32>(familyIdx);
			}
		}

		if (m_asyncComputeFamilyIdx == idxNone<Uint32>
#if VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			|| m_asyncComputeFamilyIdx == m_gfxFamilyIdx
#endif // VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			)
		{
			if ((queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
			{
				m_asyncComputeFamilyIdx = static_cast<Uint32>(familyIdx);
			}
		}

		if (m_transferFamilyIdx == idxNone<Uint32> 
#if VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			|| m_transferFamilyIdx == m_gfxFamilyIdx || m_transferFamilyIdx == m_asyncComputeFamilyIdx
#endif // VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			)
		{
			if ((queueFlags & VK_QUEUE_TRANSFER_BIT) != 0)
			{
				m_transferFamilyIdx = static_cast<Uint32>(familyIdx);
			}
		}
	}

	SPT_CHECK(m_gfxFamilyIdx != idxNone<Uint32>);
	SPT_CHECK(m_asyncComputeFamilyIdx != idxNone<Uint32>);
	SPT_CHECK(m_transferFamilyIdx != idxNone<Uint32>);

	const auto createQueueInfo = [](Uint32 familyIdx) -> VkDeviceQueueCreateInfo
	{
		static const float s_queuePriority = 1.f;

		VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queueInfo.queueFamilyIndex = familyIdx;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &s_queuePriority;
		return queueInfo;
	};

	lib::DynamicArray<VkDeviceQueueCreateInfo> queuesInfo;
	queuesInfo.emplace_back(createQueueInfo(m_gfxFamilyIdx));

	if (m_asyncComputeFamilyIdx != m_gfxFamilyIdx)
	{
		queuesInfo.emplace_back(createQueueInfo(m_asyncComputeFamilyIdx));
	}

	if (m_transferFamilyIdx != m_gfxFamilyIdx && m_transferFamilyIdx != m_asyncComputeFamilyIdx)
	{
		queuesInfo.emplace_back(createQueueInfo(m_transferFamilyIdx));
	}

	return queuesInfo;
}

VkQueue LogicalDevice::GetGfxQueueHandle() const
{
	return m_gfxQueueHandle;
}

VkQueue LogicalDevice::GetAsyncComputeQueueHandle() const
{
	return m_asyncComputeQueueHandle;
}

VkQueue LogicalDevice::GetTransferQueueHandle() const
{
	return m_transferQueueHandle;
}

VkQueue LogicalDevice::GetQueueHandle(rhi::ECommandBufferQueueType queueType) const
{
	switch (queueType)
	{
	case rhi::ECommandBufferQueueType::Graphics:			return GetGfxQueueHandle();
	case rhi::ECommandBufferQueueType::AsyncCompute:		return GetAsyncComputeQueueHandle();
	case rhi::ECommandBufferQueueType::Transfer:			return GetTransferQueueHandle();
	}

	SPT_CHECK_NO_ENTRY();
	return VK_NULL_HANDLE;
}

Uint32 LogicalDevice::GetGfxQueueFamilyIdx() const
{
	return m_gfxFamilyIdx;
}

Uint32 LogicalDevice::GetAsyncComputeQueueFamilyIdx() const
{
	return m_asyncComputeFamilyIdx;
}

Uint32 LogicalDevice::GetQueueFamilyIdx(rhi::ECommandBufferQueueType queueType) const
{
	switch (queueType)
	{
	case rhi::ECommandBufferQueueType::Graphics:			return GetGfxQueueFamilyIdx();
	case rhi::ECommandBufferQueueType::AsyncCompute:		return GetAsyncComputeQueueFamilyIdx();
	case rhi::ECommandBufferQueueType::Transfer:			return GetTransferQueueFamilyIdx();
	}

	SPT_CHECK_NO_ENTRY();
	return 0;
}

Uint32 LogicalDevice::GetTransferQueueFamilyIdx() const
{
	return m_transferFamilyIdx;
}

}
