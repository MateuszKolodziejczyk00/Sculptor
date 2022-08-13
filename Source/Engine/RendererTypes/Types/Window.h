#pragma once

#include "RendererTypesMacros.h"
#include "RHIBridge/RHIWindowImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "Window/PlatformWindowImpl.h"
#include "UITypes.h"


namespace spt::renderer
{

class Semaphore;
class Texture;
class SemaphoresArray;


class RENDERER_TYPES_API Window : public RendererResource<rhi::RHIWindow>
{
protected:

	using ResourceType = RendererResource<rhi::RHIWindow>;

public:

	Window(lib::StringView name, math::Vector2u resolution);

	Bool											ShouldClose() const;

	void											BeginFrame();
	void											Update(Real32 deltaTime);

	lib::SharedPtr<Texture>							AcquireNextSwapchainTexture(const lib::SharedPtr<Semaphore>& acquireSemaphore, Uint64 timeout = maxValue<Uint64>);

	void											PresentTexture(const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	Bool											IsSwapchainOutOfDate() const;
	void											RebuildSwapchain();

	void											InitializeUI(ui::UIContext context);
	void											UninitializeUI();

	ui::UIContext									GetUIContext() const;

	math::Vector2u									GetFramebufferSize() const;

private:

	lib::UniquePtr<platform::PlatformWindow>		m_platformWindow;
	rhi::RHIWindow									m_rhiWindow;

	Uint32											m_acquiredImageIdx;
};

}