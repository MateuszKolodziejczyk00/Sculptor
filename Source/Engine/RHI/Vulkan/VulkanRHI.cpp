#include "VulkanRHI.h"
#include "Device/PhysicalDevice.h"
#include "Device/LogicalDevice.h"
#include "VulkanRHILimits.h"
#include "Debug/DebugMessenger.h"
#include "Memory/VulkanMemoryManager.h"
#include "CommandPool/RHICommandPoolsManager.h"
#include "VulkanTypes/RHISemaphore.h"
#include "VulkanTypes/RHICommandBuffer.h"
#include "VulkanUtils.h"
#include "LayoutsManager.h"
#include "Pipeline/PipelineLayoutsManager.h"
#include "DescriptorSets/RHIDescriptorSetManager.h"
#include "Engine.h"

#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"

#include "Vulkan/Debug/GPUCrashTracker.h"

#include "Logging/Log.h"


namespace spt::vulkan
{

SPT_DEFINE_LOG_CATEGORY(VulkanRHI, true);

namespace priv
{

// VulkanInstanceData ============================================================================

class VulkanInstanceData
{
public:

	VulkanInstanceData()
		: instance(VK_NULL_HANDLE)
		, physicalDevice(VK_NULL_HANDLE)
		, debugMessenger(VK_NULL_HANDLE)
	{ }

	VkInstance                  instance;

	VkPhysicalDevice            physicalDevice;
	LogicalDevice               device;

	VulkanMemoryManager         memoryManager;
	
	VkDebugUtilsMessengerEXT    debugMessenger;

	CommandPoolsManager         commandPoolsManager;

	LayoutsManager              layoutsManager;

	PipelineLayoutsManager      pipelineLayoutsManager;

	rhi::RHISettings            rhiSettings;
};

static VulkanInstanceData g_data;

// Volk ==========================================================================================

void InitializeVolk()
{
	volkInitialize();
}

void VolkLoadInstance(VkInstance instance)
{
	volkLoadInstance(instance);
}

} // priv

// Vulkan RHI ====================================================================================

void VulkanRHI::Initialize(const rhi::RHIInitializationInfo& initInfo)
{
	priv::g_data.rhiSettings.Initialize();

	priv::InitializeVolk();

	VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "Sculptor";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "Sculptor";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = &applicationInfo;

	// Extensions
	const auto getExtensionNamePtr = [](std::string_view extensionName)
	{
		return extensionName.data();
	};

	lib::DynamicArray<const char*> extensionNames;
	extensionNames.reserve(initInfo.extensionsNum);

	for (Uint32 i = 0; i < initInfo.extensionsNum; ++i)
	{
		extensionNames.emplace_back(initInfo.extensions[i]);
	}

	extensionNames.emplace_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
	extensionNames.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#if SPT_RHI_DEBUG

	extensionNames.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	extensionNames.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#endif // SPT_RHI_DEBUG

	instanceInfo.enabledExtensionCount   = static_cast<Uint32>(extensionNames.size());
	instanceInfo.ppEnabledExtensionNames = !extensionNames.empty() ? extensionNames.data() : VK_NULL_HANDLE;

	lib::DynamicArray<const char*> enabledLayers;

#if SPT_RHI_DEBUG
	if (GetSettings().IsValidationEnabled())
	{
		enabledLayers.emplace_back(VULKAN_VALIDATION_LAYER_NAME);
	}
#endif // SPT_RHI_DEBUG

	// query supported layers
	Uint32 supportedLayersCount = 0;
	vkEnumerateInstanceLayerProperties(&supportedLayersCount, nullptr);
	lib::DynamicArray<VkLayerProperties> supportedLayers(supportedLayersCount);
	vkEnumerateInstanceLayerProperties(&supportedLayersCount, supportedLayers.data());

	for (const char* layer : enabledLayers)
	{
		const auto layerIt = std::find_if(supportedLayers.begin(), supportedLayers.end(),
										  [layer](const VkLayerProperties& layerProps)
										  {
											  return strcmp(layer, layerProps.layerName) == 0;
										  });

		if (layerIt == supportedLayers.end())
		{
			SPT_LOG_FATAL(VulkanRHI, "Requested layer '{0}' is not supported!", layer);
		}
	}

	instanceInfo.enabledLayerCount = static_cast<Uint32>(enabledLayers.size());
	instanceInfo.ppEnabledLayerNames = enabledLayers.data();

	VulkanStructsLinkedList instanceInfoLinkedList(instanceInfo);

#if SPT_RHI_DEBUG

	const VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = DebugMessenger::CreateDebugMessengerInfo();

	if (GetSettings().IsValidationEnabled())
	{
		instanceInfoLinkedList.Append(debugMessengerInfo);
	}

	lib::DynamicArray<VkValidationFeatureEnableEXT> enabledValidationFeatures;

	if (GetSettings().IsStrictValidationEnabled())
	{
		enabledValidationFeatures.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
		enabledValidationFeatures.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
		enabledValidationFeatures.emplace_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
		enabledValidationFeatures.emplace_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);

		VkValidationFeaturesEXT validationFeatures{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
		validationFeatures.enabledValidationFeatureCount = static_cast<Uint32>(enabledValidationFeatures.size());
		validationFeatures.pEnabledValidationFeatures = enabledValidationFeatures.data();

		instanceInfoLinkedList.Append(validationFeatures);
	}

#endif // SPT_RHI_DEBUG

	SPT_VK_CHECK(vkCreateInstance(&instanceInfo, GetAllocationCallbacks(), &priv::g_data.instance));

	priv::VolkLoadInstance(priv::g_data.instance);

#if SPT_RHI_DEBUG

	if (GetSettings().IsValidationEnabled())
	{
		priv::g_data.debugMessenger = DebugMessenger::CreateDebugMessenger(priv::g_data.instance, GetAllocationCallbacks());
	}

#endif // SPT_RHI_DEBUG

	priv::g_data.pipelineLayoutsManager.InitializeRHI();

#if SPT_ENABLE_GPU_CRASH_DUMPS
	if (GetSettings().AreGPUCrashDumpsEnabled())
	{
		GPUCrashTracker::EnableGPUCrashDumps();
	}
#endif // SPT_ENABLE_GPU_CRASH_DUMPS
}

void VulkanRHI::InitializeGPUForWindow(IntPtr windowSurfaceHandle)
{
	SPT_CHECK(!!priv::g_data.instance);

	const VkSurfaceKHR surface = reinterpret_cast<VkSurfaceKHR>(windowSurfaceHandle);
	SPT_CHECK(!!surface);

	priv::g_data.physicalDevice = PhysicalDevice::SelectPhysicalDevice(priv::g_data.instance, surface);

	if (SPT_IS_LOG_CATEGORY_ENABLED(VulkanRHI))
	{
		const VkPhysicalDeviceProperties2 deviceProps = PhysicalDevice::GetDeviceProperties(priv::g_data.physicalDevice);
		SPT_LOG_TRACE(VulkanRHI, "Selected Device: {0}", deviceProps.properties.deviceName);
	}

	priv::g_data.device.CreateDevice(priv::g_data.physicalDevice, GetAllocationCallbacks());

	VulkanRHILimits::Initialize(GetLogicalDevice(), GetPhysicalDeviceHandle());

	priv::g_data.memoryManager.Initialize(priv::g_data.instance, priv::g_data.device.GetHandle(), priv::g_data.physicalDevice, GetAllocationCallbacks());

	RHIDescriptorSetManager::GetInstance().InitializeRHI();
}

void VulkanRHI::Uninitialize()
{
	RHIDescriptorSetManager::GetInstance().ReleaseRHI();

	priv::g_data.commandPoolsManager.DestroyResources();

	priv::g_data.pipelineLayoutsManager.ReleaseRHI();

	if (priv::g_data.memoryManager.IsValid())
	{
		priv::g_data.memoryManager.Destroy();
	}

	if (priv::g_data.device.IsValid())
	{
		priv::g_data.device.Destroy(GetAllocationCallbacks());
	}

	if (priv::g_data.debugMessenger)
	{
		DebugMessenger::DestroyDebugMessenger(priv::g_data.debugMessenger, priv::g_data.instance, GetAllocationCallbacks());
		priv::g_data.debugMessenger = VK_NULL_HANDLE;
	}

	if (priv::g_data.instance)
	{
		vkDestroyInstance(priv::g_data.instance, GetAllocationCallbacks());
		priv::g_data.instance = VK_NULL_HANDLE;
	}
}

void VulkanRHI::BeginFrame()
{
	priv::g_data.pipelineLayoutsManager.FlushPendingPipelineLayouts();
}

rhi::ERHIType VulkanRHI::GetRHIType()
{
	return rhi::ERHIType::Vulkan_1_3;
}

void VulkanRHI::WaitIdle()
{
	priv::g_data.device.WaitIdle();
}

const rhi::RHISettings& VulkanRHI::GetSettings()
{
	return priv::g_data.rhiSettings;
}

Bool VulkanRHI::IsRayTracingEnabled()
{
	return GetSettings().IsRayTracingEnabled();
}

RHIDeviceQueue VulkanRHI::GetDeviceQueue(rhi::EDeviceCommandQueueType queueType)
{
	return priv::g_data.device.GetQueue(queueType);
}

RHIDescriptorSet VulkanRHI::AllocateDescriptorSet(const RHIDescriptorSetLayout& layout)
{
	return RHIDescriptorSetManager::GetInstance().AllocateDescriptorSet(layout);
}

void VulkanRHI::FreeDescriptorSet(const RHIDescriptorSet& set)
{
	RHIDescriptorSetManager::GetInstance().FreeDescriptorSet(set);
}

#if SPT_RHI_DEBUG

void VulkanRHI::EnableValidationWarnings(Bool enable)
{
	DebugMessenger::EnableWarnings(enable);
}

#endif // SPT_RHI_DEBUG

VkInstance VulkanRHI::GetInstanceHandle()
{
	return priv::g_data.instance;
}

VkDevice VulkanRHI::GetDeviceHandle()
{
	return priv::g_data.device.GetHandle();
}

VkPhysicalDevice VulkanRHI::GetPhysicalDeviceHandle()
{
	return priv::g_data.physicalDevice;
}

CommandPoolsManager& VulkanRHI::GetCommandPoolsManager()
{
	return priv::g_data.commandPoolsManager;
}

LayoutsManager& VulkanRHI::GetLayoutsManager()
{
	return priv::g_data.layoutsManager;
}

PipelineLayoutsManager& VulkanRHI::GetPipelineLayoutsManager()
{
	return priv::g_data.pipelineLayoutsManager;
}

const LogicalDevice& VulkanRHI::GetLogicalDevice()
{
	return priv::g_data.device;
}

VulkanMemoryManager& VulkanRHI::GetMemoryManager()
{
	return priv::g_data.memoryManager;
}

VmaAllocator VulkanRHI::GetAllocatorHandle()
{
	return priv::g_data.memoryManager.GetAllocatorHandle();
}

const VkAllocationCallbacks* VulkanRHI::GetAllocationCallbacks()
{
	return nullptr;
}

} // spt::vulkan
