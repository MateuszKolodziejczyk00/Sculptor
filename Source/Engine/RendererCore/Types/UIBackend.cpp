#include "UIBackend.h"
#include "Window.h"

namespace spt::rdr
{

UIBackend::UIBackend(ui::UIContext context, const lib::SharedRef<Window>& window)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(context.IsValid());

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
