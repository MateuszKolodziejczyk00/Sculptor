#include "RHIWindow.h"
#include "VulkanRHI.h"
#include "Device/LogicalDevice.h"
#include "RHIInitialization.h"
#include "Logging/Log.h"

#include "imgui_impl_vulkan.h"


namespace spt::vulkan
{

SPT_IMPLEMENT_LOG_CATEGORY(VulkanRHIWindow, true);

RHIWindow::RHIWindow()
	: m_swapchain(VK_NULL_HANDLE)
{ }

void RHIWindow::InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo)
{
	SPT_PROFILE_FUNCTION();

	m_minImagesNum = windowInfo.m_minImageCount;

	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();
	const VkPhysicalDevice physicalDeviceHandle = VulkanRHI::GetPhysicalDeviceHandle();
	const VkSurfaceKHR surfaceHandle = VulkanRHI::GetSurfaceHandle();
	const VkInstance instanceHandle = VulkanRHI::GetInstanceHandle();

	m_surface = surfaceHandle;

	VkBool32 isSurfaceSupportedByPhysicalDevice = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR(physicalDeviceHandle, device.GetGfxQueueIdx(), m_surface, &isSurfaceSupportedByPhysicalDevice);
	if (isSurfaceSupportedByPhysicalDevice != VK_TRUE)
	{
		SPT_LOG_FATAL(VulkanRHIWindow, "Surface not supported by physical device");
		SPT_CHECK_NO_ENTRY();
	}

	const VkFormat requestSurfaceImageFormats[] =
	{
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM
	};
	const VkColorSpaceKHR requestedColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	m_surfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDeviceHandle, m_surface, requestSurfaceImageFormats, SPT_ARRAY_SIZE(requestSurfaceImageFormats), requestedColorSpace);

	const VkPresentModeKHR requestedPresentModes[] = { VK_PRESENT_MODE_FIFO_KHR };

	m_presentMode = ImGui_ImplVulkanH_SelectPresentMode(physicalDeviceHandle, surfaceHandle, requestedPresentModes, SPT_ARRAY_SIZE(requestedPresentModes));

	m_swapchain = CreateSwapchain(windowInfo.m_framebufferSize, m_swapchain);
}

void RHIWindow::ReleaseRHI()
{
	if (m_swapchain)
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), m_swapchain, VulkanRHI::GetAllocationCallbacks());
		m_swapchain = VK_NULL_HANDLE;
	}
}

VkSwapchainKHR RHIWindow::CreateSwapchain(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain)
{
	SPT_PROFILE_FUNCTION();

	VkSwapchainCreateInfoKHR swapchainInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchainInfo.surface = m_surface;
	swapchainInfo.minImageCount = m_minImagesNum;
	swapchainInfo.imageFormat = m_surfaceFormat.format;
	swapchainInfo.imageColorSpace = m_surfaceFormat.colorSpace;
	swapchainInfo.imageExtent.width = framebufferSize.x();
	swapchainInfo.imageExtent.height = framebufferSize.y();
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = m_presentMode;
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.oldSwapchain = oldSwapchain;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateSwapchainKHR(VulkanRHI::GetDeviceHandle(), &swapchainInfo, VulkanRHI::GetAllocationCallbacks(), &swapchain));

	if (oldSwapchain)
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), oldSwapchain, VulkanRHI::GetAllocationCallbacks());
	}

	return swapchain;
}

}
