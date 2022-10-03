#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIUIBackendImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "UITypes.h"


namespace spt::rdr
{

class Window;


class RENDERER_CORE_API UIBackend : public RendererResource<rhi::RHIUIBackend>
{
protected:

	using ResourceType = RendererResource<rhi::RHIUIBackend>;

public:

	UIBackend(ui::UIContext context, const lib::SharedRef<Window>& window);

	void						BeginFrame();

	void						DestroyFontsTemporaryObjects();
};

}