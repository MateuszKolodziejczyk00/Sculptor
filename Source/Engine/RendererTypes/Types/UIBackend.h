#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIUIBackendImpl.h"
#include "UITypes.h"


namespace spt::renderer
{

class Window;


class UIBackend
{
public:

	UIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window);
	~UIBackend();

	rhi::RHIUIBackend&			GetRHI();
	const rhi::RHIUIBackend&	GetRHI() const;

private:

	rhi::RHIUIBackend			m_rhiBackend;
};

}