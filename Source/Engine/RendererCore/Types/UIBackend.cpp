#include "UIBackend.h"
#include "Window.h"
#include "CurrentFrameContext.h"

namespace spt::rdr
{

void UIBackend::Initialize(ui::UIContext context, const lib::SharedRef<Window>& window)
{
	GetRHI().InitializeRHI(context, window->GetRHI());
}

void UIBackend::Uninitialize()
{
	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda([rhiBackend = GetRHI()]() mutable
																	{
																		rhiBackend.ReleaseRHI();
																	});
}

Bool UIBackend::IsValid()
{
	return GetRHI().IsValid();
}

void UIBackend::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	GetRHI().BeginFrame();
}

void UIBackend::DestroyFontsTemporaryObjects()
{
	SPT_PROFILER_FUNCTION();

	GetRHI().DestroyFontsTemporaryObjects();
}

rhi::RHIUIBackend& UIBackend::GetRHI()
{
	return GetInstance().m_rhiBackend;
}

UIBackend& UIBackend::GetInstance()
{
	static UIBackend backendInstance;
	return backendInstance;
}

} // spt::rdr
