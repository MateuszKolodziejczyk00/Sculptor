#include "Debug/DebugMessenger.h"
#include "SculptorCoreTypes.h"
#include "Logging/Log.h"


namespace spt::vulkan
{

SPT_IMPLEMENT_LOG_CATEGORY(VulkanValidation, true)

namespace priv
{

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	SPT_PROFILE_FUNCTION();

	if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		SPT_LOG_TRACE(VulkanValidation, pCallbackData->pMessage);
	}
	else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
	{
		SPT_LOG_INFO(VulkanValidation, pCallbackData->pMessage);
	}
	else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		SPT_LOG_WARN(VulkanValidation, pCallbackData->pMessage);
	}
	else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		SPT_LOG_ERROR(VulkanValidation, pCallbackData->pMessage);
		SPT_CHECK_NO_ENTRY();
	}

	return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback( VkDebugReportFlagsEXT          flags,
													VkDebugReportObjectTypeEXT     objectType,
													uint64_t                       object,
													size_t                         location,
													int32_t                        messageCode,
													const char*                    pLayerPrefix,
													const char*                    pMessage,
													void*                          pUserData)
{
	SPT_PROFILE_FUNCTION();

	if (flags == VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		SPT_LOG_ERROR(VulkanValidation, pMessage);
		SPT_CHECK_NO_ENTRY();
	}
	else if (flags == VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		SPT_LOG_WARN(VulkanValidation, pMessage);
	}
	else if (flags == VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		SPT_LOG_WARN(VulkanValidation, pMessage);
	}
	else if (flags == VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
		SPT_LOG_INFO(VulkanValidation, pMessage);
	}
	else if (flags == VK_DEBUG_REPORT_DEBUG_BIT_EXT)
	{
		SPT_LOG_INFO(VulkanValidation, pMessage);
	}
	
	return VK_FALSE;
}

}

VkDebugUtilsMessengerCreateInfoEXT DebugMessenger::CreateDebugMessengerInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT messengerInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
							   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
							   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
						   | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
						   | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	messengerInfo.pfnUserCallback = priv::DebugMessengerCallback;
	messengerInfo.pUserData = nullptr;


	return messengerInfo;
}

VkDebugUtilsMessengerEXT DebugMessenger::CreateDebugMessenger(VkInstance instance, const VkAllocationCallbacks* allocator)
{
	const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = CreateDebugMessengerInfo();

	VkDebugUtilsMessengerEXT messengerHandle = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, allocator, &messengerHandle));

	return messengerHandle;
}

void DebugMessenger::DestroyDebugMessenger(VkDebugUtilsMessengerEXT messenger, VkInstance instance, const VkAllocationCallbacks* allocator)
{
	vkDestroyDebugUtilsMessengerEXT(instance, messenger, allocator);
}

VkDebugReportCallbackEXT DebugMessenger::CreateDebugReportCallback(VkInstance instance, const VkAllocationCallbacks* allocator)
{
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackInfo{ VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
	debugReportCallbackInfo.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT
                                 && VK_DEBUG_REPORT_WARNING_BIT_EXT
                                 && VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT 
                                 && VK_DEBUG_REPORT_ERROR_BIT_EXT
                                 && VK_DEBUG_REPORT_DEBUG_BIT_EXT;
    debugReportCallbackInfo.pfnCallback = &priv::DebugReportCallback;

	VkDebugReportCallbackEXT callbackHandle = VK_NULL_HANDLE;
	vkCreateDebugReportCallbackEXT(instance, &debugReportCallbackInfo, allocator, &callbackHandle);

	return callbackHandle;
}

void DebugMessenger::DestroyDebugReportCallback(VkInstance instance, VkDebugReportCallbackEXT debugCallback, const VkAllocationCallbacks* allocator)
{
	vkDestroyDebugReportCallbackEXT(instance, debugCallback, allocator);
}

}
