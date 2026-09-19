#include "UIBackend.h"
#include "Window.h"
#include "Texture.h"
#include "ResourcesManager.h"

namespace spt::rdr
{

void UIBackend::Initialize(ui::UIContext context, const lib::SharedRef<Window>& window)
{
	GetRHI().InitializeRHI(context, window->GetRHI());
}

void UIBackend::Uninitialize()
{
	GetRHI().ReleaseRHI();
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

ui::TextureID UIBackend::GetUITextureID(const lib::SharedRef<TextureView>& texture, rhi::ESamplerFilterType filterType /*= rhi::ESamplerFilterType::Linear*/)
{
	SPT_PROFILER_FUNCTION();

	return GetRHI().GetUITexture(texture->GetRHI(), filterType);
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
