#include "UIBackend.h"
#include "Window.h"
#include "CurrentFrameContext.h"
#include "Texture.h"
#include "Sampler.h"

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

ui::TextureID UIBackend::GetUITextureID(const lib::SharedRef<TextureView>& texture, const lib::SharedRef<Sampler>& sampler)
{
	SPT_PROFILER_FUNCTION();

	return GetRHI().GetUITexture(texture->GetRHI(), sampler->GetRHI());
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
