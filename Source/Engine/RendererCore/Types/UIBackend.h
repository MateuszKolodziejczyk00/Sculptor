#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIUIBackendImpl.h"
#include "SculptorCoreTypes.h"
#include "UIContext.h"
#include "UITypes.h"
#include "RHICore/RHISamplerTypes.h"


namespace spt::rdr
{

class Window;
class TextureView;
class Sampler;


class RENDERER_CORE_API UIBackend
{
public:

	static void Initialize(ui::UIContext context, const lib::SharedRef<Window>& window);
	static void Uninitialize();

	static Bool IsValid();

	static void	BeginFrame();

	static void	DestroyFontsTemporaryObjects();

	static ui::TextureID GetUITextureID(const lib::SharedRef<TextureView>& texture, const lib::SharedRef<Sampler>& sampler);
	static ui::TextureID GetUITextureID(const lib::SharedRef<TextureView>& texture,
										rhi::ESamplerFilterType filterType = rhi::ESamplerFilterType::Linear,
										rhi::EMipMapAddressingMode mipMapAddressing = rhi::EMipMapAddressingMode::Nearest,
										rhi::EAxisAddressingMode axisAddressing = rhi::EAxisAddressingMode::Repeat);

	static rhi::RHIUIBackend& GetRHI();
	
private:

	UIBackend() = default;

	static UIBackend& GetInstance();

	rhi::RHIUIBackend m_rhiBackend;
};

} // spt::rdr