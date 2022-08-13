#include "Window.h"
#include "RendererUtils.h"
#include "RHICore/RHIInitialization.h"
#include "RHIBridge/RHIImpl.h"
#include "Semaphore.h"
#include "Texture.h"

namespace spt::renderer
{

Window::Window(lib::StringView name, math::Vector2u resolution)
	: m_platformWindow(std::make_unique<platform::PlatformWindow>(name, resolution))
{
	rhi::RHI::InitializeGPUForWindow();

	rhi::RHIWindowInitializationInfo initInfo;
	initInfo.m_framebufferSize = m_platformWindow->GetFramebufferSize();
	initInfo.m_minImageCount = RendererUtils::GetFramesInFlightNum();

	GetRHI().InitializeRHI(initInfo);
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

	m_acquiredImageIdx = GetRHI().AcquireSwapchainImage(rhiSemaphore, timeout);

	lib::SharedPtr<Texture> acquiredTexture;

	if (!GetRHI().IsSwapchainOutOfDate())
	{
		const rhi::RHITexture acquiredRHITexture = GetRHI().GetSwapchinImage(m_acquiredImageIdx);

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

	GetRHI().PresentSwapchainImage(rhiWaitSemaphores, m_acquiredImageIdx);
}

Bool Window::IsSwapchainOutOfDate() const
{
	return GetRHI().IsSwapchainOutOfDate();
}

void Window::RebuildSwapchain()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsSwapchainOutOfDate());

	const math::Vector2u framebufferSize = m_platformWindow->GetFramebufferSize();

	GetRHI().RebuildSwapchain(framebufferSize);
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
