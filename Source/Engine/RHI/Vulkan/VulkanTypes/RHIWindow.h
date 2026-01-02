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


struct RHI_API RHIWindowReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkSwapchainKHR> swapchain;

	RHIResourceReleaseTicket<VkSurfaceKHR> surface;
};


class RHI_API RHIWindow
{
public:

	RHIWindow();

	RHIWindow(RHIWindow&& rhs);
	RHIWindow& operator=(RHIWindow&& rhs);

	RHIWindow(const RHIWindow& rhs) = delete;
	RHIWindow&					operator=(const RHIWindow& rhs) = delete;

	void						InitializeRHI(const rhi::RHIWindowInitializationInfo& windowInfo, Uint32 minImagesCount, IntPtr surfaceHandle);
	void						ReleaseRHI();

	RHIWindowReleaseTicket		DeferredReleaseRHI();

	Bool						IsValid() const;
	Bool						IsSwapchainValid() const;

	Uint32						AcquireSwapchainImage(const RHISemaphore& acquireSemaphore, Uint64 timeout = idxNone<Uint64>);
	RHITexture					GetSwapchinImage(Uint32 imageIdx) const;

	rhi::EFragmentFormat		GetFragmentFormat() const;

	Uint32						GetSwapchainImagesNum() const;

	Bool						PresentSwapchainImage(const RHIDeviceQueue& queue, const lib::DynamicArray<RHISemaphore>& waitSemaphores, Uint32 imageIdx);

	Bool						IsSwapchainOutOfDate() const;
	void						RebuildSwapchain(math::Vector2u framebufferSize, IntPtr surfaceHandle);

	math::Vector2u				GetSwapchainSize() const;

	Bool						IsVSyncEnabled() const;
	void						SetVSyncEnabled(Bool newValue);

	void						SetSwapchainOutOfDate();

	// Vulkan Specific ============================================

	VkFormat					GetSurfaceFormat() const;

	VkSwapchainKHR				GetSwapchainHandle() const;

private:

	void						ReleaseSwapchain();

	VkSwapchainKHR				CreateSwapchain_Locked(math::Vector2u framebufferSize, VkSwapchainKHR oldSwapchain);

	void						CacheSwapchainImages_Locked(VkSwapchainKHR m_swapchain);

	VkSwapchainKHR				m_swapchain;

	lib::DynamicArray<VkImage>	m_swapchainImages;

	rhi::TextureDefinition		m_swapchainTextureDef;

	VkSurfaceKHR				m_surface;

	VkSurfaceFormatKHR			m_surfaceFormat;

	Uint32						m_minImagesNum;
	Bool						m_enableVSync;

	Bool						m_swapchainOutOfDate;

	math::Vector2u				m_swapchainSize;

	mutable lib::Lock			m_swapchainLock;
};

} // spt::vulkan