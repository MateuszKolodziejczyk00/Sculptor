#include "RHIWindow.h"
#include "Vulkan/VulkanRHI.h"
#include "RHISemaphore.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "RHICore/RHIInitialization.h"
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
	, m_swapchainSize(0, 0)
{ }

RHIWindow::RHIWindow(RHIWindow&& rhs)
	: m_swapchain(rhs.m_swapchain)
	, m_swapchainImages(std::move(rhs.m_swapchainImages))
	, m_swapchainTextureDef(rhs.m_swapchainTextureDef)
	, m_surface(rhs.m_surface)
	, m_presentMode(rhs.m_presentMode)
	, m_surfaceFormat(rhs.m_surfaceFormat)
	, m_minImagesNum(rhs.m_minImagesNum)
	, m_swapchainOutOfDate(rhs.m_swapchainOutOfDate)
	, m_swapchainSize(rhs.m_swapchainSize)
{
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
	m_swapchainSize			= rhs.m_swapchainSize;

	rhs.m_swapchain = VK_NULL_HANDLE;

	return *this;
}

void RHIWindow::InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsValid());

	m_minImagesNum = windowInfo.minImageCount;

	const LogicalDevice& device						= VulkanRHI::GetLogicalDevice();
	const VkPhysicalDevice physicalDeviceHandle		= VulkanRHI::GetPhysicalDeviceHandle();
	const VkSurfaceKHR surfaceHandle				= VulkanRHI::GetSurfaceHandle();

	const Bool success = ImGui_ImplVulkan_LoadFunctions(&priv::LoadVulkanFunction, nullptr);
	SPT_CHECK(success);

	m_surface = surfaceHandle;

	VkSurfaceCapabilitiesKHR surfaceCapabilities{ VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT };
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDeviceHandle, m_surface, &surfaceCapabilities);

	const Uint32 framebufferWidth	= windowInfo.framebufferSize.x();
	const Uint32 framebufferHeight	= windowInfo.framebufferSize.y();

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

	RebuildSwapchain(windowInfo.framebufferSize);
}

void RHIWindow::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!!IsValid());

	vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), m_swapchain, VulkanRHI::GetAllocationCallbacks());
	m_swapchain = VK_NULL_HANDLE;
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

	SPT_CHECK(!IsSwapchainOutOfDate());

	Uint32 imageIdx = idxNone<Uint32>;
	const VkResult result = vkAcquireNextImageKHR(VulkanRHI::GetDeviceHandle(), m_swapchain, timeout, acquireSemaphore.GetHandle(), nullptr, &imageIdx);
	
	SPT_CHECK(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR);

	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		SetSwapchainOutOfDate();
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

Uint32 RHIWindow::GetSwapchainImagesNum() const
{
	SPT_CHECK(IsValid());

	return static_cast<Uint32>(m_swapchainImages.size());
}

Bool RHIWindow::PresentSwapchainImage(const lib::DynamicArray<RHISemaphore>& waitSemaphores, Uint32 imageIdx)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!IsSwapchainOutOfDate());

	lib::DynamicArray<VkSemaphore> waitSemaphoreHandles(waitSemaphores.size());
	for (SizeType idx = 0; idx < waitSemaphores.size(); ++idx)
	{
		waitSemaphoreHandles[idx] = waitSemaphores[idx].GetHandle();
	}

	VkResult result = VK_SUCCESS;

	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = static_cast<Uint32>(waitSemaphoreHandles.size());
    presentInfo.pWaitSemaphores = waitSemaphoreHandles.data();
    presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIdx;
    presentInfo.pResults = &result;

	vkQueuePresentKHR(VulkanRHI::GetLogicalDevice().GetGfxQueueHandle(), &presentInfo);
	
	SPT_CHECK(result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		SetSwapchainOutOfDate();
	}

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

math::Vector2u RHIWindow::GetSwapchainSize() const
{
	return m_swapchainSize;
}

VkFormat RHIWindow::GetSurfaceFormat() const
{
	return m_surfaceFormat.format;
}

VkSwapchainKHR RHIWindow::CreateSwapchain(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain)
{
	SPT_PROFILE_FUNCTION();

	const VkFormat swapchainTextureFormat			= m_surfaceFormat.format;
	const VkImageUsageFlags swapchainTextureUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const rhi::EFragmentFormat rhiSwapchainTextureFormat	= RHITexture::GetRHIFormat(swapchainTextureFormat);
	const rhi::ETextureUsage rhiSwapchainTextureUsage		= RHITexture::GetRHITextureUsageFlags(swapchainTextureUsage);

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

	m_swapchainTextureDef.resolution	= math::Vector3u(framebufferSize.x(), framebufferSize.y(), 1);
	m_swapchainTextureDef.usage			= rhiSwapchainTextureUsage;
	m_swapchainTextureDef.format		= rhiSwapchainTextureFormat;
	m_swapchainTextureDef.samples		= 1;
	m_swapchainTextureDef.mipLevels		= 1;
	m_swapchainTextureDef.arrayLayers	= 1;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateSwapchainKHR(VulkanRHI::GetDeviceHandle(), &swapchainInfo, VulkanRHI::GetAllocationCallbacks(), &swapchain));

	if (oldSwapchain)
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), oldSwapchain, VulkanRHI::GetAllocationCallbacks());
	}

	m_swapchainSize = framebufferSize;

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

void RHIWindow::SetSwapchainOutOfDate()
{
	m_swapchainOutOfDate = true;
}

}
