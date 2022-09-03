#include "UIBackend.h"
#include "Window.h"

namespace spt::rdr
{

UIBackend::UIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(context.IsValid() && !!window);

	GetRHI().InitializeRHI(context, window->GetRHI());
}

void UIBackend::BeginFrame()
{
	GetRHI().BeginFrame();
}

void UIBackend::DestroyFontsTemporaryObjects()
{
	GetRHI().DestroyFontsTemporaryObjects();
}

}
