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

	static void									Initialize(Uint32 framesInFlightNum);
	static void									Shutdown();

	static void									BeginFrame();
	static void									EndFrame();

	using CleanupDelegate						= lib::ThreadSafeMulticastDelegate<>;
	static CleanupDelegate&						GetCurrentFrameCleanupDelegate();

	static void									ReleaseAllResources();

	static Uint64								GetCurrentFrameIdx();

	static const lib::SharedPtr<Semaphore>&		GetReleaseFrameSemaphore();

private:

	static void									FlushCurrentFrameReleases();
};

}