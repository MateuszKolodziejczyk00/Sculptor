#include "UIBackend.h"
#include "Window.h"
#include "CurrentFrameContext.h"

namespace spt::renderer
{

UIBackend::UIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(context.IsValid() && !!window);

	m_rhiBackend.InitializeRHI(context, window->GetRHI());
}

UIBackend::~UIBackend()
{
	CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
		[resource = m_rhiBackend]() mutable
		{
			resource.ReleaseRHI();
		});
}

spt::rhi::RHIUIBackend& UIBackend::GetRHI()
{
	return m_rhiBackend;
}

const spt::rhi::RHIUIBackend& UIBackend::GetRHI() const
{
	return m_rhiBackend;
}

}
