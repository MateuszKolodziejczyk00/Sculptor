#include "Renderer.h"
#include "RendererUtils.h"
#include "CurrentFrameContext.h"
#include "Window/PlatformWindowImpl.h"
#include "RHIInitialization.h"
#include "RHIImpl.h"


namespace spt::renderer
{

void Renderer::Initialize()
{
	platform::PlatformWindow::Initialize();

	CurrentFrameContext::Initialize(RendererUtils::GetFramesInFlightNum());

	const platform::RequiredExtensionsInfo requiredExtensions = platform::PlatformWindow::GetRequiredRHIExtensionNames();

	rhi::RHIInitializationInfo initializationInfo;
	initializationInfo.m_extensions = requiredExtensions.m_extensions;
	initializationInfo.m_extensionsNum = requiredExtensions.m_extensionsNum;

	rhi::RHI::Initialize(initializationInfo);
}

void Renderer::Shutdown()
{
	CurrentFrameContext::Shutdown();

	rhi::RHI::Uninitialize();
}

void Renderer::BeginFrame()
{
	CurrentFrameContext::BeginFrame();
}

void Renderer::EndFrame()
{
	CurrentFrameContext::EndFrame();
}

}
