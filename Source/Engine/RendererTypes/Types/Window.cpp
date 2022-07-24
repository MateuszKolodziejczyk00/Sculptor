#include "Window.h"
#include "Window/PlatformWindowImpl.h"
#include "CurrentFrameContext.h"
#include "RendererUtils.h"
#include "RHIInitialization.h"
#include "RHIImpl.h"

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
	CurrentFrameContext::SubmitDeferredRelease(
		[resource = m_rhiWindow]() mutable
		{
			resource.ReleaseRHI();
		});
}

Bool Window::ShouldClose() const
{
	return m_platformWindow->ShouldClose();
}

void Window::Update(Real32 deltaTime)
{
	SPT_PROFILE_FUNCTION();

	m_platformWindow->Update(deltaTime);
}

}
