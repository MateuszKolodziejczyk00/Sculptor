#include "Window.h"
#include "RendererSettings.h"
#include "RHICore/RHIInitialization.h"
#include "RHIBridge/RHIImpl.h"
#include "Semaphore.h"
#include "Texture.h"

namespace spt::rdr
{

Window::Window(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo)
	: m_platformWindow(std::make_unique<platf::PlatformWindow>(name, windowInfo.framebufferSize))
{
	const IntPtr surfaceHandle = m_platformWindow->GetRHISurfaceHandle();

	rhi::RHI::InitializeGPUForWindow(surfaceHandle);

	GetRHI().InitializeRHI(windowInfo, RendererSettings::Get().framesInFlight, surfaceHandle);

	m_platformWindow->GetOnResizedCallback().AddRawMember(this, &Window::OnWindowResized);
	m_platformWindow->OnRHISurfaceUpdateCallback().AddRawMember(this, &Window::OnRHISurfaceUpdate);
}

Bool Window::ShouldClose() const
{
	return m_platformWindow->ShouldClose();
}

void Window::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	m_platformWindow->BeginFrame();
}

void Window::Update()
{
	SPT_PROFILER_FUNCTION();

	m_platformWindow->Update();
}

SwapchainTextureHandle Window::AcquireNextSwapchainTexture(const lib::SharedPtr<Semaphore>& acquireSemaphore, Uint64 timeout /*= maxValue<Uint64>*/)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHISemaphore rhiSemaphore = acquireSemaphore ? acquireSemaphore->GetRHI() : rhi::RHISemaphore();

	const Uint32 imageIdx = GetRHI().AcquireSwapchainImage(rhiSemaphore, timeout);

	return SwapchainTextureHandle(imageIdx, acquireSemaphore);
}

lib::SharedPtr<Texture> Window::GetSwapchainTexture(SwapchainTextureHandle handle) const
{
	SPT_PROFILER_FUNCTION();

	lib::SharedPtr<Texture> texture;

	if (handle.IsValid())
	{
		texture = lib::MakeShared<Texture>(RENDERER_RESOURCE_NAME("SwapchainImage"), GetRHI().GetSwapchinImage(handle.GetImageIdx()));
	}

	return texture;
}

void Window::PresentTexture(SwapchainTextureHandle handle, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(handle.IsValid());

	lib::DynamicArray<rhi::RHISemaphore> rhiWaitSemaphores(waitSemaphores.size());
	for (SizeType idx = 0; idx < waitSemaphores.size(); ++idx)
	{
		rhiWaitSemaphores[idx] = waitSemaphores[idx]->GetRHI();
	}

	GetRHI().PresentSwapchainImage(rhiWaitSemaphores, handle.GetImageIdx());
}

Bool Window::IsSwapchainOutOfDate() const
{
	return GetRHI().IsSwapchainOutOfDate();
}

Bool Window::RebuildSwapchain()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsSwapchainOutOfDate());

	m_platformWindow->UpdateRHISurface();

	const math::Vector2u framebufferSize = m_platformWindow->GetFramebufferSize();

	GetRHI().RebuildSwapchain(framebufferSize, m_platformWindow->GetRHISurfaceHandle());

	return IsSwapchainValid();
}

Bool Window::IsSwapchainValid() const
{
	return GetRHI().IsSwapchainValid();
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

math::Vector2u Window::GetSwapchainSize() const
{
	return GetRHI().GetSwapchainSize();
}

void Window::OnWindowResized(Uint32 newWidth, Uint32 newHeight)
{
	GetRHI().SetSwapchainOutOfDate();
}

void Window::OnRHISurfaceUpdate(IntPtr prevSufaceHandle, IntPtr newSurfaceHandle)
{
	if (prevSufaceHandle != newSurfaceHandle)
	{
		GetRHI().SetSwapchainOutOfDate();
	}
}

} // spt::rdr
