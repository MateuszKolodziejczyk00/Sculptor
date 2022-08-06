#pragma once

#include "VulkanRHIMacros.h"
#include "SculptorCoreTypes.h"
#include "Vulkan.h"
#include "RHITexture.h"


namespace spt::rhi
{
struct RHIWindowInitializationInfo;
}


namespace spt::vulkan
{

class RHISemaphore;
class RHISemaphoresArray;


class VULKAN_RHI_API RHIWindow
{
public:

	RHIWindow();

	RHIWindow(RHIWindow&& rhs);
	RHIWindow&					operator=(RHIWindow&& rhs);

	RHIWindow(const RHIWindow& rhs) = delete;
	RHIWindow&					operator=(const RHIWindow& rhs) = delete;

	void						InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo);
	void						ReleaseRHI();

	Bool						IsValid() const;

	void						BeginFrame();

	Uint32						AcquireSwapchainImage(const RHISemaphore& acquireSemaphore, Uint64 timeout = idxNone<Uint64>);
	RHITexture					GetSwapchinImage(Uint32 imageIdx) const;

	Uint32						GetSwapchainImagesNum() const;

	Bool						PresentSwapchainImage(const lib::DynamicArray<RHISemaphore>& waitSemaphores, Uint32 imageIdx);

	Bool						IsSwapchainOutOfDate() const;
	void						RebuildSwapchain(math::Vector2u framebufferSize);

	// Vulkan Specific ============================================

	VkFormat					GetSurfaceFormat() const;

private:

	VkSwapchainKHR				CreateSwapchain(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain);

	void						CacheSwapchainImages(VkSwapchainKHR m_swapchain);

	void						SetSwapchainOutOfDate();

	VkSwapchainKHR				m_swapchain;

	lib::DynamicArray<VkImage>	m_swapchainImages;

	rhi::TextureDefinition		m_swapchainTextureDef;

	VkSurfaceKHR				m_surface;

	VkPresentModeKHR			m_presentMode;
	VkSurfaceFormatKHR			m_surfaceFormat;

	Uint32						m_minImagesNum;

	Bool						m_swapchainOutOfDate;
};

}