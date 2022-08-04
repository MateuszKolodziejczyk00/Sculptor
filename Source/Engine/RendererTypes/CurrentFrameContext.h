#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::renderer
{

class Semaphore;


class RENDERER_TYPES_API CurrentFrameContext
{
public:

	static void									Initialize(Uint32 framesInFlightNum);
	static void									Shutdown();

	static void									BeginFrame();
	static void									EndFrame();

	using CleanupDelegate						= lib::ThreadsafeMulticastDelegate<>;
	static CleanupDelegate&						GetCurrentFrameCleanupDelegate();

	static void									ReleaseAllResources();

	static Uint64								GetCurrentFrameIdx();

	static const lib::SharedPtr<Semaphore>&		GetReleaseFrameSemaphore();

private:

	static void									FlushCurrentFrameReleases();
};

}