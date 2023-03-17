#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rdr
{

class Semaphore;


class RENDERER_CORE_API CurrentFrameContext
{
public:

	static void Initialize(Uint32 framesInFlightNum);
	static void Shutdown();

	static void	WaitForFrameRendered(Uint64 frameIdx);

	using CleanupDelegate						= lib::ThreadSafeMulticastDelegate<void()>;
	static CleanupDelegate&						GetCurrentFrameCleanupDelegate();

	static void									ReleaseAllResources();

	static const lib::SharedPtr<Semaphore>&		GetReleaseFrameSemaphore();

private:

	static CleanupDelegate& GetDelegateForFrame(Uint64 frameIdx);
	static void FlushFrameReleases(Uint64 frameIdx);
};

}