#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Vulkan/VulkanUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "RHICore/RHIPlugin.h"


namespace spt::vulkan
{

namespace priv
{

Bool IsExtensionAllowed(const rhi::Extension& extension)
{
	return extension.GetStringView() != VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
}

} // priv

LogicalDevice::LogicalDevice()
	: m_deviceHandle(VK_NULL_HANDLE)
	, m_gfxFamilyIdx(idxNone<Uint32>)
	, m_asyncComputeFamilyIdx(idxNone<Uint32>)
	, m_transferFamilyIdx(idxNone<Uint32>)
{ }

void LogicalDevice::CreateDevice(VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator)
{
	SPT_PROFILER_FUNCTION();

	const lib::Span<const rhi::Extension> deviceExtensions = rhi::RHIPluginsManager::Get().GetDeviceExtensions();

	lib::DynamicArray<const char*> extensionPtrs;
	extensionPtrs.reserve(deviceExtensions.size());
	for (const rhi::Extension& extension : deviceExtensions)
	{
		if (priv::IsExtensionAllowed(extension))
		{
			extensionPtrs.emplace_back(extension.GetCStr());
		}
	}

	const lib::DynamicArray<VkDeviceQueueCreateInfo> queueInfos = CreateQueueInfos(physicalDevice);

	VkDeviceCreateInfo deviceInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.queueCreateInfoCount = static_cast<Uint32>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();

	if (!extensionPtrs.empty())
	{
		deviceInfo.enabledExtensionCount = static_cast<Uint32>(extensionPtrs.size());
		deviceInfo.ppEnabledExtensionNames = extensionPtrs.data();
	}

	VulkanStructsLinkedList deviceInfoLinkedData(deviceInfo);

	VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	features.features.multiViewport				= VK_TRUE;
	features.features.samplerAnisotropy			= VK_TRUE;
	features.features.sampleRateShading			= VK_TRUE;
	features.features.independentBlend			= VK_TRUE;
	features.features.fillModeNonSolid			= VK_TRUE;
	features.features.shaderInt16				= VK_TRUE;
	features.features.shaderInt64				= VK_TRUE;
	features.features.pipelineStatisticsQuery	= VK_TRUE;
	features.features.fragmentStoresAndAtomics	= VK_TRUE;
	features.features.shaderImageGatherExtended = VK_TRUE;
	features.features.shaderStorageImageReadWithoutFormat	= VK_TRUE;
	features.features.shaderStorageImageWriteWithoutFormat	= VK_TRUE;

	deviceInfoLinkedData.Append(features);

	VkPhysicalDeviceVulkan11Features vulkan11Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	vulkan11Features.shaderDrawParameters     = VK_TRUE;
	vulkan11Features.storageBuffer16BitAccess = VK_TRUE;

	deviceInfoLinkedData.Append(vulkan11Features);

	VkPhysicalDeviceVulkan12Features vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	vulkan12Features.bufferDeviceAddress	= VK_TRUE;
	vulkan12Features.timelineSemaphore		= VK_TRUE;
	vulkan12Features.drawIndirectCount		= VK_TRUE;
	vulkan12Features.shaderFloat16			= VK_TRUE;
	vulkan12Features.shaderBufferInt64Atomics = VK_TRUE;
	vulkan12Features.shaderSharedInt64Atomics = VK_TRUE;
	vulkan12Features.descriptorBindingSampledImageUpdateAfterBind	= VK_TRUE;
	vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind	= VK_TRUE;
	vulkan12Features.descriptorBindingStorageImageUpdateAfterBind	= VK_TRUE;
	vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind	= VK_TRUE;
	vulkan12Features.descriptorBindingPartiallyBound				= VK_TRUE;
	vulkan12Features.shaderSampledImageArrayNonUniformIndexing		= VK_TRUE;
	vulkan12Features.samplerFilterMinmax			= VK_TRUE;
	vulkan12Features.hostQueryReset					= VK_TRUE;
	vulkan12Features.vulkanMemoryModel				= VK_TRUE;
	vulkan12Features.vulkanMemoryModelDeviceScope	= VK_TRUE;
	vulkan12Features.runtimeDescriptorArray = VK_TRUE;
	vulkan12Features.scalarBlockLayout      = VK_TRUE;

	deviceInfoLinkedData.Append(vulkan12Features);

	VkPhysicalDeviceVulkan13Features vulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	vulkan13Features.synchronization2 = VK_TRUE;
	vulkan13Features.dynamicRendering = VK_TRUE;
	vulkan13Features.shaderDemoteToHelperInvocation	= VK_TRUE;

	deviceInfoLinkedData.Append(vulkan13Features);

	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
	meshShaderFeatures.taskShader        = VK_TRUE;
	meshShaderFeatures.meshShader        = VK_TRUE;
	meshShaderFeatures.meshShaderQueries = VK_TRUE;

	deviceInfoLinkedData.Append(meshShaderFeatures);

	VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipelineExecutablePropertiesFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR };
	pipelineExecutablePropertiesFeatures.pipelineExecutableInfo = VK_TRUE;

	deviceInfoLinkedData.Append(pipelineExecutablePropertiesFeatures);

	VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR relaxedExtendedInstructionFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR };
	relaxedExtendedInstructionFeatures.shaderRelaxedExtendedInstruction = VK_TRUE;

	deviceInfoLinkedData.Append(relaxedExtendedInstructionFeatures);

	VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR computeShaderDerivativesFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR };
	computeShaderDerivativesFeatures.computeDerivativeGroupLinear = VK_TRUE;
	computeShaderDerivativesFeatures.computeDerivativeGroupQuads  = VK_TRUE;

	deviceInfoLinkedData.Append(computeShaderDerivativesFeatures);

	VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableDescriptorTypeFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT };
	mutableDescriptorTypeFeatures.mutableDescriptorType = VK_TRUE;

	deviceInfoLinkedData.Append(mutableDescriptorTypeFeatures);

	VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR };
	unifiedImageLayoutsFeatures.unifiedImageLayouts = VK_TRUE;

	deviceInfoLinkedData.Append(unifiedImageLayoutsFeatures);

#if SPT_ENABLE_NSIGHT_AFTERMATH
	VkDeviceDiagnosticsConfigCreateInfoNV aftermathInfo{ VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV };

	if (VulkanRHI::GetSettings().AreGPUCrashDumpsEnabled())
	{
		const VkDeviceDiagnosticsConfigFlagsNV aftermathFlags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV
															  | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV
															  | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV;
		aftermathInfo.flags = aftermathFlags;
		
		deviceInfoLinkedData.Append(aftermathInfo);
	}
#endif // SPT_ENABLE_NSIGHT_AFTERMATH

	const Bool rayTracingEnabled = VulkanRHI::GetSettings().IsRayTracingEnabled();

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	if (rayTracingEnabled)
	{
		accelerationStructureFeatures.accelerationStructure = VK_TRUE;
		deviceInfoLinkedData.Append(accelerationStructureFeatures);
	}

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
	if (rayTracingEnabled)
	{
		rayTracingPipelineFeatures.rayTracingPipeline					= VK_TRUE;
		rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect	= VK_TRUE;
		rayTracingPipelineFeatures.rayTraversalPrimitiveCulling			= VK_TRUE;
		deviceInfoLinkedData.Append(rayTracingPipelineFeatures);
	}

	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	if (rayTracingEnabled)
	{
		rayQueryFeatures.rayQuery = VK_TRUE;
		deviceInfoLinkedData.Append(rayQueryFeatures);
	}

	VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
	descriptorBufferFeatures.descriptorBuffer                   = VK_TRUE;
	descriptorBufferFeatures.descriptorBufferImageLayoutIgnored = VK_TRUE;

	deviceInfoLinkedData.Append(descriptorBufferFeatures);

	SPT_VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, allocator, &m_deviceHandle));

	VkQueue gfxQueueHandle{ VK_NULL_HANDLE };
	VkDeviceQueueInfo2 gfxQueueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
	gfxQueueInfo.queueFamilyIndex = m_gfxFamilyIdx;
	gfxQueueInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_deviceHandle, &gfxQueueInfo, &gfxQueueHandle);
	m_gfxQueue.InitializeRHI(rhi::EDeviceCommandQueueType::Graphics, gfxQueueHandle);

	VkQueue asyncComputeQueueHandle{ VK_NULL_HANDLE };
	VkDeviceQueueInfo2 asyncComputeQueueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
	asyncComputeQueueInfo.queueFamilyIndex = m_asyncComputeFamilyIdx;
	asyncComputeQueueInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_deviceHandle, &asyncComputeQueueInfo, &asyncComputeQueueHandle);
	m_asyncComputeQueue.InitializeRHI(rhi::EDeviceCommandQueueType::AsyncCompute, asyncComputeQueueHandle);

	VkQueue transferQueueHandle{ VK_NULL_HANDLE };
	VkDeviceQueueInfo2 transferQueueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
	transferQueueInfo.queueFamilyIndex = m_transferFamilyIdx;
	transferQueueInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_deviceHandle, &transferQueueInfo, &transferQueueHandle);
	m_transferQueue.InitializeRHI(rhi::EDeviceCommandQueueType::Transfer, transferQueueHandle);

	volkLoadDevice(m_deviceHandle);

	VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
	VkPhysicalDeviceProperties2 physicalDeviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	physicalDeviceProps.pNext = &descriptorProps;

	vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProps);

	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::Sampler)]                = static_cast<Uint32>(descriptorProps.samplerDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::CombinedTextureSampler)] = static_cast<Uint32>(descriptorProps.combinedImageSamplerDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::SampledTexture)]         = static_cast<Uint32>(descriptorProps.sampledImageDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::StorageTexture)]         = static_cast<Uint32>(descriptorProps.storageImageDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::UniformTexelBuffer)]     = static_cast<Uint32>(descriptorProps.uniformTexelBufferDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::StorageTexelBuffer)]     = static_cast<Uint32>(descriptorProps.storageTexelBufferDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::UniformBuffer)]          = static_cast<Uint32>(descriptorProps.uniformBufferDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::StorageBuffer)]          = static_cast<Uint32>(descriptorProps.storageBufferDescriptorSize);
	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::AccelerationStructure)]  = static_cast<Uint32>(descriptorProps.accelerationStructureDescriptorSize);

	m_descriptorProps.descriptorsAlignment = static_cast<Uint32>(descriptorProps.descriptorBufferOffsetAlignment);

	Uint32 mutableDescriptorSize = 0u;
	mutableDescriptorSize = std::max(mutableDescriptorSize, m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::UniformBuffer)]);
	mutableDescriptorSize = std::max(mutableDescriptorSize, m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::StorageBuffer)]);
	mutableDescriptorSize = std::max(mutableDescriptorSize, m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::SampledTexture)]);
	mutableDescriptorSize = std::max(mutableDescriptorSize, m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::StorageTexture)]);

	SPT_CHECK(mutableDescriptorSize > 0u);

	m_descriptorProps.sizes[static_cast<Uint32>(rhi::EDescriptorType::CBV_SRV_UAV)] = mutableDescriptorSize;
}

void LogicalDevice::Destroy(const VkAllocationCallbacks* allocator)
{
	SPT_CHECK(IsValid());

	m_gfxQueue.ReleaseRHI();

	if (m_asyncComputeQueue.IsValid())
	{
		m_asyncComputeQueue.ReleaseRHI();
	}

	if (m_transferQueue.IsValid())
	{
		m_transferQueue.ReleaseRHI();
	}

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
#if SPT_VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			|| m_asyncComputeFamilyIdx == m_gfxFamilyIdx
#endif // SPT_VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			)
		{
			if ((queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
			{
				m_asyncComputeFamilyIdx = static_cast<Uint32>(familyIdx);
			}
		}

		if (m_transferFamilyIdx == idxNone<Uint32> 
#if SPT_VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
			|| m_transferFamilyIdx == m_gfxFamilyIdx || m_transferFamilyIdx == m_asyncComputeFamilyIdx
#endif // SPT_VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES
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

RHIDeviceQueue LogicalDevice::GetGfxQueue() const
{
	return m_gfxQueue;
}

RHIDeviceQueue LogicalDevice::GetAsyncComputeQueue() const
{
	return m_asyncComputeQueue;
}

RHIDeviceQueue LogicalDevice::GetTransferQueue() const
{
	return m_transferQueue;
}

RHIDeviceQueue LogicalDevice::GetQueue(rhi::EDeviceCommandQueueType queueType) const
{
	switch (queueType)
	{
	case rhi::EDeviceCommandQueueType::Graphics:			return GetGfxQueue();
	case rhi::EDeviceCommandQueueType::AsyncCompute:		return GetAsyncComputeQueue();
	case rhi::EDeviceCommandQueueType::Transfer:			return GetTransferQueue();
	}

	SPT_CHECK_NO_ENTRY();
	return RHIDeviceQueue();
}

Uint32 LogicalDevice::GetGfxQueueFamilyIdx() const
{
	return m_gfxFamilyIdx;
}

Uint32 LogicalDevice::GetAsyncComputeQueueFamilyIdx() const
{
	return m_asyncComputeFamilyIdx;
}

Uint32 LogicalDevice::GetQueueFamilyIdx(rhi::EDeviceCommandQueueType queueType) const
{
	switch (queueType)
	{
	case rhi::EDeviceCommandQueueType::Graphics:			return GetGfxQueueFamilyIdx();
	case rhi::EDeviceCommandQueueType::AsyncCompute:		return GetAsyncComputeQueueFamilyIdx();
	case rhi::EDeviceCommandQueueType::Transfer:			return GetTransferQueueFamilyIdx();
	}

	SPT_CHECK_NO_ENTRY();
	return 0;
}

Uint32 LogicalDevice::GetTransferQueueFamilyIdx() const
{
	return m_transferFamilyIdx;
}

}
