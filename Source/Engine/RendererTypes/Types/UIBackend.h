#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHIUIBackendImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "UITypes.h"


namespace spt::renderer
{

class Window;


class RENDERER_TYPES_API UIBackend : public RendererResource<rhi::RHIUIBackend>
{
protected:

	using ResourceType = RendererResource<rhi::RHIUIBackend>;

public:

	UIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window);

	void						BeginFrame();

	void						DestroyFontsTemporaryObjects();
};

}