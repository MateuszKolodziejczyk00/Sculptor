#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIWindowImpl.h"
#include "Window/PlatformWindowFwd.h"


namespace spt::renderer
{

class Semaphore;
class Texture;
class SemaphoresArray;


class RENDERER_TYPES_API Window
{
public:

	Window(lib::StringView name, math::Vector2u resolution);
	~Window();

	rhi::RHIWindow&									GetRHI();
	const rhi::RHIWindow&							GetRHI() const;

	Bool											ShouldClose() const;

	void											BeginFrame();
	void											Update(Real32 deltaTime);

	lib::SharedPtr<Texture>							AcquireNextSwapchainTexture(const lib::SharedPtr<Semaphore>& acquireSemaphore, Uint64 timeout = maxValue<Uint64>);

	void											PresentTexture(const SemaphoresArray& waitSemaphores);

private:

	lib::UniquePtr<platform::PlatformWindow>		m_platformWindow;
	rhi::RHIWindow									m_rhiWindow;

	Uint32											m_acquiredImageIdx;
};

}