#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIWindowImpl.h"
#include "Window/PlatformWindowFwd.h"


namespace spt::renderer
{

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

private:

	lib::UniquePtr<platform::PlatformWindow>		m_platformWindow;
	rhi::RHIWindow									m_rhiWindow;
};

}