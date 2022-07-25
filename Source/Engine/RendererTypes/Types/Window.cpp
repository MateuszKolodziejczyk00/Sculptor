#include "Window.h"
#include "Window/PlatformWindowImpl.h"
#include "CurrentFrameContext.h"
#include "RendererUtils.h"
#include "RHIInitialization.h"
#include "RHIImpl.h"
#include "Semaphore.h"
#include "Texture.h"

namespace spt::renderer
{

Window::Window(lib::StringView name, math::Vector2u resolution)
{
	m_platformWindow = std::make_unique<platform::PlatformWindow>(name, resolution);

	rhi::RHI::InitializeGPUForWindow();

	rhi::RHIWindowInitializationInfo initInfo;
	initInfo.m_framebufferSize = m_platformWindow->GetFramebufferSize();
	initInfo.m_minImageCount = RendererUtils::GetFramesInFlightNum();

	m_rhiWindow.InitializeRHI(initInfo);
}

Window::~Window()
{
	SPT_PROFILE_FUNCTION();

	CurrentFrameContext::SubmitDeferredRelease(
		[resource = std::move(m_rhiWindow)]() mutable
		{
			resource.ReleaseRHI();
		});
}

rhi::RHIWindow& Window::GetRHI()
{
	return m_rhiWindow;
}

const rhi::RHIWindow& Window::GetRHI() const
{
	return m_rhiWindow;
}

Bool Window::ShouldClose() const
{
	return m_platformWindow->ShouldClose();
}

void Window::BeginFrame()
{
	SPT_PROFILE_FUNCTION();

	m_platformWindow->BeginFrame();
}

void Window::Update(Real32 deltaTime)
{
	SPT_PROFILE_FUNCTION();

	m_platformWindow->Update(deltaTime);
}

lib::SharedPtr<Texture> Window::AcquireNextSwapchainTexture(const lib::SharedPtr<Semaphore>& acquireSemaphore, Uint64 timeout /*= maxValue<Uint64>*/)
{
	SPT_PROFILE_FUNCTION();

	const rhi::RHISemaphore rhiSemaphore = acquireSemaphore ? acquireSemaphore->GetRHI() : rhi::RHISemaphore();

	const Uint32 acquiredTextureIdx = m_rhiWindow.AcquireSwapchainImage(rhiSemaphore, timeout);
	const rhi::RHITexture acquiredRHITexture = m_rhiWindow.GetSwapchinImage(acquiredTextureIdx);

	const lib::SharedPtr<Texture> acquiredTexture = std::make_shared<Texture>(RENDERER_RESOURCE_NAME("SwapchainImage"), acquiredRHITexture);

	return acquiredTexture;
}

}
