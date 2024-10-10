#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIWindowImpl.h"
#include "SculptorCoreTypes.h"
#include "RendererResource.h"
#include "Window/PlatformWindowImpl.h"
#include "UITypes.h"
#include "Utility/NamedType.h"


namespace spt::rdr
{

class Semaphore;
class Texture;
class SemaphoresArray;


class SwapchainTextureHandle
{
public:

	SwapchainTextureHandle()
		: m_imageIdx{ idxNone<Uint32> }
	{ }

	SwapchainTextureHandle(Uint32 imageIdx, lib::SharedPtr<Semaphore> acquireSemaphore)
		: m_imageIdx{ imageIdx }
		, m_acquireSemaphore{ std::move(acquireSemaphore) }
	{ }

	Bool IsValid() const
	{
		return m_imageIdx != idxNone<Uint32> && m_acquireSemaphore;
	}

	const lib::SharedPtr<Semaphore>& GetAcquireSemaphore() const
	{
		return m_acquireSemaphore;
	}

	Uint32 GetImageIdx() const
	{
		return m_imageIdx;
	}

private:

	lib::SharedPtr<rdr::Semaphore> m_acquireSemaphore;
	Uint32 m_imageIdx;
};


class RENDERER_CORE_API Window : public RendererResource<rhi::RHIWindow>
{
protected:

	using ResourceType = RendererResource<rhi::RHIWindow>;

public:

	Window(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo);

	Bool						ShouldClose() const;

	Bool						IsFocused() const;

	void						BeginFrame();
	void						Update();

	SwapchainTextureHandle		AcquireNextSwapchainTexture(const lib::SharedPtr<Semaphore>& acquireSemaphore, Uint64 timeout = maxValue<Uint64>);

	lib::SharedPtr<Texture>		GetSwapchainTexture(SwapchainTextureHandle handle) const;

	void						PresentTexture(SwapchainTextureHandle handle, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	Bool						IsSwapchainOutOfDate() const;
	Bool						RebuildSwapchain();

	Bool						IsSwapchainValid() const;

	void						InitializeUI(ui::UIContext context);
	void						UninitializeUI();

	ui::UIContext				GetUIContext() const;

	math::Vector2u				GetSwapchainSize() const;

private:

	void						OnWindowResized(Uint32 newWidth, Uint32 newHeight);
	void						OnRHISurfaceUpdate(IntPtr prevSufaceHandle, IntPtr newSurfaceHandle);

	lib::UniquePtr<platf::PlatformWindow> m_platformWindow;

	// extend lifetime of this semaphore until we present same image second time
	// normally we're going to destroy all resources from this frame after flip workload will finish
	// but these semaphores must be valid until present is done (spec is a bit loose here but it's better to be safe)
	lib::DynamicArray<lib::DynamicArray<lib::SharedPtr<Semaphore>>> m_presentWaitSemaphores;
};

} // spt::rdr