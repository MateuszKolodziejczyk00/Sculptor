#pragma once

#include "VulkanRHIMacros.h"
#include "SculptorCoreTypes.h"
#include "Vulkan.h"


namespace spt::rhi
{
struct RHIWindowInitializationInfo;
}


namespace spt::vulkan
{

class VULKAN_RHI_API RHIWindow
{
public:

	RHIWindow();

	void						InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo);
	void						ReleaseRHI();

private:

	VkSwapchainKHR				CreateSwapchain(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain);

	VkSwapchainKHR				m_swapchain;

	VkSurfaceKHR				m_surface;

	VkPresentModeKHR			m_presentMode;
	VkSurfaceFormatKHR			m_surfaceFormat;

	Uint32						m_minImagesNum;
};

}