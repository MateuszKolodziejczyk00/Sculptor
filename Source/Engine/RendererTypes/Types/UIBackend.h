#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIUIBackendImpl.h"
#include "UITypes.h"


namespace spt::renderer
{

class Window;


class RENDERER_TYPES_API UIBackend
{
public:

	UIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window);
	~UIBackend();

	rhi::RHIUIBackend&			GetRHI();
	const rhi::RHIUIBackend&	GetRHI() const;

	void						BeginFrame();

	void						DestroyFontsTemporaryObjects();

private:

	rhi::RHIUIBackend			m_rhiBackend;
};

}