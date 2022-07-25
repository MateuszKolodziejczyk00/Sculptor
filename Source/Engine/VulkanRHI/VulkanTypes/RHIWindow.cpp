#include "RHIWindow.h"
#include "VulkanRHI.h"
#include "RHISemaphore.h"
#include "Device/LogicalDevice.h"
#include "RHIInitialization.h"
#include "Logging/Log.h"

#include "imgui_impl_vulkan.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

SPT_IMPLEMENT_LOG_CATEGORY(VulkanRHIWindow, true);

namespace priv
{

PFN_vkVoidFunction LoadVulkanFunction(const char* functionName, void* userData)
{
	const VkDevice deviceHandle = static_cast<VkDevice>(userData);

	PFN_vkVoidFunction func = nullptr;
	func = vkGetDeviceProcAddr(VulkanRHI::GetDeviceHandle(), functionName);

	if (!func)
	{
		func = vkGetInstanceProcAddr(VulkanRHI::GetInstanceHandle(), functionName);
	}

	SPT_CHECK(!!func);

	return func;
}
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIWindow =====================================================================================

RHIWindow::RHIWindow()
	: m_swapchain(VK_NULL_HANDLE)
	, m_surface(VK_NULL_HANDLE)
	, m_minImagesNum(idxNone<Uint32>)
	, m_swapchainOutOfDate(false)
{ }

RHIWindow::RHIWindow(RHIWindow&& rhs)
{
	m_swapchain				= rhs.m_swapchain;
	m_swapchainImages		= std::move(rhs.m_swapchainImages);
	m_swapchainTextureDef	= rhs.m_swapchainTextureDef;
	m_surface				= rhs.m_surface;
	m_presentMode			= rhs.m_presentMode;
	m_surfaceFormat			= rhs.m_surfaceFormat;
	m_minImagesNum			= rhs.m_minImagesNum;
	m_swapchainOutOfDate	= rhs.m_swapchainOutOfDate;

	rhs.m_swapchain = VK_NULL_HANDLE;
}

RHIWindow& RHIWindow::operator=(RHIWindow&& rhs)
{
	m_swapchain				= rhs.m_swapchain;
	m_swapchainImages		= std::move(rhs.m_swapchainImages);
	m_swapchainTextureDef	= rhs.m_swapchainTextureDef;
	m_surface				= rhs.m_surface;
	m_presentMode			= rhs.m_presentMode;
	m_surfaceFormat			= rhs.m_surfaceFormat;
	m_minImagesNum			= rhs.m_minImagesNum;
	m_swapchainOutOfDate	= rhs.m_swapchainOutOfDate;

	rhs.m_swapchain = VK_NULL_HANDLE;

	return *this;
}

void RHIWindow::InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_minImagesNum = windowInfo.m_minImageCount;

	const LogicalDevice& device						= VulkanRHI::GetLogicalDevice();
	const VkDevice deviceHandle						= device.GetHandle();
	const VkPhysicalDevice physicalDeviceHandle		= VulkanRHI::GetPhysicalDeviceHandle();
	const VkSurfaceKHR surfaceHandle				= VulkanRHI::GetSurfaceHandle();
	const VkInstance instanceHandle					= VulkanRHI::GetInstanceHandle();

	const Bool success = ImGui_ImplVulkan_LoadFunctions(&priv::LoadVulkanFunction, nullptr);
	SPT_CHECK(success);

	m_surface = surfaceHandle;

	VkSurfaceCapabilitiesKHR surfaceCapabilities{ VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT };
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDeviceHandle, m_surface, &surfaceCapabilities);

	const Uint32 framebufferWidth	= windowInfo.m_framebufferSize.x();
	const Uint32 framebufferHeight	= windowInfo.m_framebufferSize.y();

	SPT_CHECK(framebufferWidth >= surfaceCapabilities.minImageExtent.width && framebufferWidth <= surfaceCapabilities.maxImageExtent.width);
	SPT_CHECK(framebufferHeight >= surfaceCapabilities.minImageExtent.height && framebufferHeight <= surfaceCapabilities.maxImageExtent.height);

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

	RebuildSwapchain(windowInfo.m_framebufferSize);
}

void RHIWindow::ReleaseRHI()
{
	if (m_swapchain)
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), m_swapchain, VulkanRHI::GetAllocationCallbacks());
		m_swapchain = VK_NULL_HANDLE;
	}
}

Bool RHIWindow::IsValid() const
{
	return m_swapchain != VK_NULL_HANDLE;
}

void RHIWindow::BeginFrame()
{
	SPT_PROFILE_FUNCTION();

}

Uint32 RHIWindow::AcquireSwapchainImage(const RHISemaphore& acquireSemaphore, Uint64 timeout /*= idxNone<Uint64>*/)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!m_swapchainOutOfDate);

	Uint32 imageIdx = idxNone<Uint32>;
	VkResult result = vkAcquireNextImageKHR(VulkanRHI::GetDeviceHandle(), m_swapchain, timeout, acquireSemaphore.GetHandle(), nullptr, &imageIdx);

	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_swapchainOutOfDate = true;
	}

	return imageIdx;
}

RHITexture RHIWindow::GetSwapchinImage(Uint32 imageIdx) const
{
	SPT_CHECK(imageIdx < static_cast<Uint32>(m_swapchainImages.size()));

	RHITexture texture;
	texture.InitializeRHI(m_swapchainTextureDef, m_swapchainImages[imageIdx]);

	return texture;
}

Bool RHIWindow::PresentSwapchainImage(const RHISemaphoresArray& waitSemaphores, Uint32 imageIdx)
{
	SPT_PROFILE_FUNCTION();

	VkResult result = VK_SUCCESS;

	VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
    timelineSemaphoreSubmitInfo.waitSemaphoreValueCount = static_cast<Uint32>(waitSemaphores.GetValues().size());
    timelineSemaphoreSubmitInfo.pWaitSemaphoreValues = waitSemaphores.GetValues().data();

	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = static_cast<Uint32>(waitSemaphores.GetSemaphores().size());
    presentInfo.pWaitSemaphores = waitSemaphores.GetSemaphores().data();
    presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIdx;
    presentInfo.pResults = &result;
	presentInfo.pNext = &timelineSemaphoreSubmitInfo;

	vkQueuePresentKHR(VulkanRHI::GetLogicalDevice().GetGfxQueueHandle(), &presentInfo);
	
	SPT_CHECK(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR);

	return result == VK_SUCCESS;
}

Bool RHIWindow::IsSwapchainOutOfDate() const
{
	return m_swapchainOutOfDate;
}

void RHIWindow::RebuildSwapchain(math::Vector2u framebufferSize)
{
	SPT_PROFILE_FUNCTION();

	m_swapchain = CreateSwapchain(framebufferSize, m_swapchain);

	CacheSwapchainImages(m_swapchain);
	 
	m_swapchainOutOfDate = false;
}

VkSwapchainKHR RHIWindow::CreateSwapchain(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain)
{
	SPT_PROFILE_FUNCTION();

	const VkFormat swapchainTextureFormat			= m_surfaceFormat.format;
	const VkImageUsageFlags swapchainTextureUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const rhi::EFragmentFormat rhiSwapchainTextureFormat	= RHITexture::GetRHIFormat(swapchainTextureFormat);
	const Flags32 rhiSwapchainTextureUsage					= RHITexture::GetRHITextureUsageFlags(swapchainTextureUsage);

	VkSwapchainCreateInfoKHR swapchainInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchainInfo.surface					= m_surface;
	swapchainInfo.minImageCount				= m_minImagesNum;
	swapchainInfo.imageFormat				= m_surfaceFormat.format;
	swapchainInfo.imageColorSpace			= m_surfaceFormat.colorSpace;
	swapchainInfo.imageExtent.width			= framebufferSize.x();
	swapchainInfo.imageExtent.height		= framebufferSize.y();
	swapchainInfo.imageArrayLayers			= 1;
	swapchainInfo.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform				= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainInfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode				= m_presentMode;
	swapchainInfo.clipped					= VK_TRUE;
	swapchainInfo.oldSwapchain				= oldSwapchain;

	m_swapchainTextureDef.m_resolution		= math::Vector3u(framebufferSize.x(), framebufferSize.y(), 1);
	m_swapchainTextureDef.m_usage			= rhiSwapchainTextureUsage;
	m_swapchainTextureDef.m_format			= rhiSwapchainTextureFormat;
	m_swapchainTextureDef.m_samples			= 1;
	m_swapchainTextureDef.m_mipLevels		= 1;
	m_swapchainTextureDef.m_arrayLayers		= 1;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateSwapchainKHR(VulkanRHI::GetDeviceHandle(), &swapchainInfo, VulkanRHI::GetAllocationCallbacks(), &swapchain));

	if (oldSwapchain)
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), oldSwapchain, VulkanRHI::GetAllocationCallbacks());
	}

	return swapchain;
}

void RHIWindow::CacheSwapchainImages(VkSwapchainKHR swapchain)
{
	SPT_PROFILE_FUNCTION();

	Uint32 imagesNum = 0;
	SPT_VK_CHECK(vkGetSwapchainImagesKHR(VulkanRHI::GetDeviceHandle(), swapchain, &imagesNum, nullptr));
	m_swapchainImages.resize(static_cast<SizeType>(imagesNum));
	SPT_VK_CHECK(vkGetSwapchainImagesKHR(VulkanRHI::GetDeviceHandle(), swapchain, &imagesNum, m_swapchainImages.data()));
}

}
