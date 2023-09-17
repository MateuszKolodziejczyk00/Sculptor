#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"
#include "RHICore/RHIInitialization.h"
#include "RHITexture.h"


namespace spt::vulkan
{

class RHISemaphore;
class RHISemaphoresArray;


class RHI_API RHIWindow
{
public:

	RHIWindow();

	RHIWindow(RHIWindow&& rhs);
	RHIWindow&					operator=(RHIWindow&& rhs);

	RHIWindow(const RHIWindow& rhs) = delete;
	RHIWindow&					operator=(const RHIWindow& rhs) = delete;

	void						InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo, Uint32 minImagesCount);
	void						ReleaseRHI();

	Bool						IsValid() const;

	void						BeginFrame();

	Uint32						AcquireSwapchainImage(const RHISemaphore& acquireSemaphore, Uint64 timeout = idxNone<Uint64>);
	RHITexture					GetSwapchinImage(Uint32 imageIdx) const;

	rhi::EFragmentFormat		GetFragmentFormat() const;

	Uint32						GetSwapchainImagesNum() const;

	Bool						PresentSwapchainImage(const lib::DynamicArray<RHISemaphore>& waitSemaphores, Uint32 imageIdx);

	Bool						IsSwapchainOutOfDate() const;
	void						RebuildSwapchain(math::Vector2u framebufferSize);

	math::Vector2u				GetSwapchainSize() const;

	Bool						IsVSyncEnabled() const;
	void						SetVSyncEnabled(Bool newValue);

	// Vulkan Specific ============================================

	VkFormat					GetSurfaceFormat() const;

	VkSwapchainKHR				GetSwapchainHandle() const;

private:

	VkSwapchainKHR				CreateSwapchain(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain);

	void						CacheSwapchainImages(VkSwapchainKHR m_swapchain);
	void						ReleaseSwapchainImages();

	void						SetSwapchainOutOfDate();

	VkSwapchainKHR				m_swapchain;

	lib::DynamicArray<VkImage>	m_swapchainImages;

	rhi::TextureDefinition		m_swapchainTextureDef;

	VkSurfaceKHR				m_surface;

	VkSurfaceFormatKHR			m_surfaceFormat;

	Uint32						m_minImagesNum;
	Bool						m_enableVSync;

	Bool						m_swapchainOutOfDate;

	math::Vector2u				m_swapchainSize;
};

} // spt::vulkan