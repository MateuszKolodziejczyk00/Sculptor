#include "RHIWindow.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/LayoutsManager.h"
#include "RHISemaphore.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "RHICore/RHIInitialization.h"
#include "Logging/Log.h"
#include "imgui_impl_vulkan.h"


namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

SPT_DEFINE_LOG_CATEGORY(VulkanRHIWindow, true);

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
// RHIWindowReleaseTicket ========================================================================

void RHIWindowReleaseTicket::ExecuteReleaseRHI()
{
	if (swapchain.IsValid())
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), swapchain.GetValue(), VulkanRHI::GetAllocationCallbacks());
		swapchain.Reset();
	}

	if (surface.IsValid())
	{
		vkDestroySurfaceKHR(VulkanRHI::GetInstanceHandle(), surface.GetValue(), VulkanRHI::GetAllocationCallbacks());
		surface.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIWindow =====================================================================================

RHIWindow::RHIWindow()
	: m_swapchain(VK_NULL_HANDLE)
	, m_surface(VK_NULL_HANDLE)
	, m_surfaceFormat{}
	, m_minImagesNum(idxNone<Uint32>)
	, m_enableVSync(false)
	, m_swapchainOutOfDate(false)
	, m_swapchainSize(0, 0)
{ }

RHIWindow::RHIWindow(RHIWindow&& rhs)
	: m_swapchain(rhs.m_swapchain)
	, m_swapchainImages(std::move(rhs.m_swapchainImages))
	, m_swapchainTextureDef(rhs.m_swapchainTextureDef)
	, m_surface(rhs.m_surface)
	, m_surfaceFormat(rhs.m_surfaceFormat)
	, m_minImagesNum(rhs.m_minImagesNum)
	, m_enableVSync(rhs.m_enableVSync)
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
	m_surfaceFormat			= rhs.m_surfaceFormat;
	m_minImagesNum			= rhs.m_minImagesNum;
	m_enableVSync			= rhs.m_enableVSync;
	m_swapchainOutOfDate	= rhs.m_swapchainOutOfDate;
	m_swapchainSize			= rhs.m_swapchainSize;

	rhs.m_swapchain = VK_NULL_HANDLE;

	return *this;
}

void RHIWindow::InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo, Uint32 minImagesCount, IntPtr surfaceHandle)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	m_minImagesNum = minImagesCount;
	m_enableVSync  = windowInfo.enableVSync;
	m_surface      = reinterpret_cast<VkSurfaceKHR>(surfaceHandle);

	const LogicalDevice& device						= VulkanRHI::GetLogicalDevice();
	const VkPhysicalDevice physicalDeviceHandle		= VulkanRHI::GetPhysicalDeviceHandle();

	const Bool success = ImGui_ImplVulkan_LoadFunctions(&priv::LoadVulkanFunction, nullptr);
	SPT_CHECK(success);

	VkSurfaceCapabilitiesKHR surfaceCapabilities{ VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT };
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDeviceHandle, m_surface, &surfaceCapabilities);

	const Uint32 framebufferWidth	= windowInfo.framebufferSize.x();
	const Uint32 framebufferHeight	= windowInfo.framebufferSize.y();

	SPT_CHECK(framebufferWidth >= surfaceCapabilities.minImageExtent.width && framebufferWidth <= surfaceCapabilities.maxImageExtent.width);
	SPT_CHECK(framebufferHeight >= surfaceCapabilities.minImageExtent.height && framebufferHeight <= surfaceCapabilities.maxImageExtent.height);

	VkBool32 isSurfaceSupportedByPhysicalDevice = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR(physicalDeviceHandle, device.GetGfxQueueFamilyIdx(), m_surface, &isSurfaceSupportedByPhysicalDevice);
	if (isSurfaceSupportedByPhysicalDevice != VK_TRUE)
	{
		SPT_LOG_FATAL(VulkanRHIWindow, "Surface not supported by physical device");
		SPT_CHECK_NO_ENTRY();
	}

	const VkFormat requestSurfaceImageFormats[] =
	{
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM
	};

	const VkColorSpaceKHR requestedColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	m_surfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDeviceHandle, m_surface, requestSurfaceImageFormats, SPT_ARRAY_SIZE(requestSurfaceImageFormats), requestedColorSpace);

	RebuildSwapchain(windowInfo.framebufferSize, reinterpret_cast<IntPtr>(m_surface));
}

void RHIWindow::ReleaseRHI()
{
	RHIWindowReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIWindowReleaseTicket RHIWindow::DeferredReleaseRHI()
{
	SPT_CHECK(IsValid());

	RHIWindowReleaseTicket releaseTicket;
	releaseTicket.surface   = m_surface;
	releaseTicket.swapchain = m_swapchain;

	{
		const lib::LockGuard lock(m_swapchainLock);

		ReleaseSwapchainImages_Locked();
	}

	m_surface   = VK_NULL_HANDLE;
	m_swapchain = VK_NULL_HANDLE;
	
	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHIWindow::IsValid() const
{
	return m_surface != VK_NULL_HANDLE;
}

Bool RHIWindow::IsSwapchainValid() const
{
	return m_swapchain != VK_NULL_HANDLE;
}

Uint32 RHIWindow::AcquireSwapchainImage(const RHISemaphore& acquireSemaphore, Uint64 timeout /*= idxNone<Uint64>*/)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsSwapchainOutOfDate());

	const lib::LockGuard lock(m_swapchainLock);

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

	const lib::LockGuard lock(m_swapchainLock);

	RHITexture texture;
	texture.InitializeRHI(m_swapchainTextureDef, m_swapchainImages[imageIdx], rhi::EMemoryUsage::GPUOnly);

	return texture;
}

rhi::EFragmentFormat RHIWindow::GetFragmentFormat() const
{
	SPT_CHECK(IsValid());

	return m_swapchainTextureDef.format;
}

Uint32 RHIWindow::GetSwapchainImagesNum() const
{
	SPT_CHECK(IsSwapchainValid());

	const lib::LockGuard lock(m_swapchainLock);

	return static_cast<Uint32>(m_swapchainImages.size());
}

Bool RHIWindow::PresentSwapchainImage(const lib::DynamicArray<RHISemaphore>& waitSemaphores, Uint32 imageIdx)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsSwapchainOutOfDate());

	const lib::LockGuard lock(m_swapchainLock);

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

	vkQueuePresentKHR(VulkanRHI::GetLogicalDevice().GetGfxQueue().GetHandleChecked(), &presentInfo);
	
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

void RHIWindow::RebuildSwapchain(math::Vector2u framebufferSize, IntPtr surfaceHandle)
{
	SPT_PROFILER_FUNCTION();

	m_swapchainSize = framebufferSize;

	const VkSurfaceKHR prevSurface = m_surface;
	m_surface = reinterpret_cast<VkSurfaceKHR>(surfaceHandle);

	if (framebufferSize.x() > 0 && framebufferSize.y() > 0)
	{
		const lib::LockGuard lock(m_swapchainLock);

		m_swapchain = CreateSwapchain_Locked(framebufferSize, m_swapchain);

		CacheSwapchainImages_Locked(m_swapchain);
	}
	else
	{
		ReleaseSwapchain();
	}

	if (m_surface != prevSurface && prevSurface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(VulkanRHI::GetInstanceHandle(), prevSurface, VulkanRHI::GetAllocationCallbacks());
	}
	 
	m_swapchainOutOfDate = false;
}

math::Vector2u RHIWindow::GetSwapchainSize() const
{
	return m_swapchainSize;
}

Bool RHIWindow::IsVSyncEnabled() const
{
	return m_enableVSync;
}

void RHIWindow::SetVSyncEnabled(Bool newValue)
{
	if (m_enableVSync != newValue)
	{
		m_enableVSync = newValue;
		m_swapchainOutOfDate = true;
	}
}

void RHIWindow::SetSwapchainOutOfDate()
{
	m_swapchainOutOfDate = true;
}

VkFormat RHIWindow::GetSurfaceFormat() const
{
	return m_surfaceFormat.format;
}

VkSwapchainKHR RHIWindow::GetSwapchainHandle() const
{
	return m_swapchain;
}

void RHIWindow::ReleaseSwapchain()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lock(m_swapchainLock);

	ReleaseSwapchainImages_Locked();

	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), m_swapchain, VulkanRHI::GetAllocationCallbacks());
		m_swapchain = VK_NULL_HANDLE;
	}
}

VkSwapchainKHR RHIWindow::CreateSwapchain_Locked(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain)
{
	SPT_PROFILER_FUNCTION();

	const VkPresentModeKHR requestedPresentModes[] = { m_enableVSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR };
	const VkPresentModeKHR presentMode = ImGui_ImplVulkanH_SelectPresentMode(VulkanRHI::GetPhysicalDeviceHandle(), m_surface, requestedPresentModes, SPT_ARRAY_SIZE(requestedPresentModes));

	const VkFormat swapchainTextureFormat			= m_surfaceFormat.format;
	const VkImageUsageFlags swapchainTextureUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
	swapchainInfo.imageUsage				= swapchainTextureUsage;
	swapchainInfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform				= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainInfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode				= presentMode;
	swapchainInfo.clipped					= VK_TRUE;
	swapchainInfo.oldSwapchain				= VK_NULL_HANDLE;

	m_swapchainTextureDef.resolution	= math::Vector3u(framebufferSize.x(), framebufferSize.y(), 1);
	m_swapchainTextureDef.usage			= rhiSwapchainTextureUsage;
	m_swapchainTextureDef.format		= rhiSwapchainTextureFormat;
	m_swapchainTextureDef.samples		= 1;
	m_swapchainTextureDef.mipLevels		= 1;
	m_swapchainTextureDef.arrayLayers	= 1;

	if (oldSwapchain)
	{
		ReleaseSwapchainImages_Locked();
		vkDestroySwapchainKHR(VulkanRHI::GetDeviceHandle(), oldSwapchain, VulkanRHI::GetAllocationCallbacks());
	}

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	SPT_VK_CHECK(vkCreateSwapchainKHR(VulkanRHI::GetDeviceHandle(), &swapchainInfo, VulkanRHI::GetAllocationCallbacks(), &swapchain));

	m_swapchainSize = framebufferSize;

	return swapchain;
}

void RHIWindow::CacheSwapchainImages_Locked(VkSwapchainKHR swapchain)
{
	SPT_PROFILER_FUNCTION();

	Uint32 imagesNum = 0;
	SPT_VK_CHECK(vkGetSwapchainImagesKHR(VulkanRHI::GetDeviceHandle(), swapchain, &imagesNum, nullptr));
	m_swapchainImages.resize(static_cast<SizeType>(imagesNum));
	SPT_VK_CHECK(vkGetSwapchainImagesKHR(VulkanRHI::GetDeviceHandle(), swapchain, &imagesNum, m_swapchainImages.data()));
}

void RHIWindow::ReleaseSwapchainImages_Locked()
{
	for (VkImage image : m_swapchainImages)
	{
		VulkanRHI::GetLayoutsManager().UnregisterImage(image);
	}
}

} // spt::vulkan
