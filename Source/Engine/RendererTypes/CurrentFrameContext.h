#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"
#include <mutex>


namespace spt::renderer
{

class RENDERER_TYPES_API CurrentFrameContext
{
public:

	static void						Initialize(Uint32 framesInFlightNum);

	static void						BeginFrame();

	static void						EndFrame();

	static void						Shutdown();

	using CleanupDelegate			= lib::ThreadsafeMulticastDelegate<>;
	static CleanupDelegate&			GetCurrentFrameCleanupDelegate();

private:

	static void						FlushCurrentFrameReleases();
	static void						FlushAllFramesReleases();
};

}