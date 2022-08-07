#include "Window.h"
#include "Window/PlatformWindowImpl.h"
#include "CurrentFrameContext.h"
#include "RendererUtils.h"
#include "RHICore/RHIInitialization.h"
#include "RHIBridge/RHIImpl.h"
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

	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
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

	m_acquiredImageIdx = m_rhiWindow.AcquireSwapchainImage(rhiSemaphore, timeout);

	lib::SharedPtr<Texture> acquiredTexture;

	if (!m_rhiWindow.IsSwapchainOutOfDate())
	{
		const rhi::RHITexture acquiredRHITexture = m_rhiWindow.GetSwapchinImage(m_acquiredImageIdx);

		acquiredTexture = std::make_shared<Texture>(RENDERER_RESOURCE_NAME("SwapchainImage"), acquiredRHITexture);
	}

	return acquiredTexture;
}

void Window::PresentTexture(const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores)
{
	SPT_PROFILE_FUNCTION();

	lib::DynamicArray<rhi::RHISemaphore> rhiWaitSemaphores(waitSemaphores.size());
	for (SizeType idx = 0; idx < waitSemaphores.size(); ++idx)
	{
		rhiWaitSemaphores[idx] = waitSemaphores[idx]->GetRHI();
	}

	m_rhiWindow.PresentSwapchainImage(rhiWaitSemaphores, m_acquiredImageIdx);
}

Bool Window::IsSwapchainOutOfDate() const
{
	return m_rhiWindow.IsSwapchainOutOfDate();
}

void Window::RebuildSwapchain()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsSwapchainOutOfDate());

	const math::Vector2u framebufferSize = m_platformWindow->GetFramebufferSize();

	m_rhiWindow.RebuildSwapchain(framebufferSize);
}

void Window::InitializeUI(ui::UIContext context)
{
	m_platformWindow->InitializeUI(context);
}

void Window::UninitializeUI()
{
	return m_platformWindow->UninitializeUI();
}

ui::UIContext Window::GetUIContext() const
{
	return m_platformWindow->GetUIContext();
}

math::Vector2u Window::GetFramebufferSize() const
{
	return m_platformWindow->GetFramebufferSize();
}

}
