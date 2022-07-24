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

	template<typename TReleaseFunctor>
	static void						SubmitDeferredRelease(TReleaseFunctor func)
	{
		SPT_PROFILE_FUNCTION();

		const std::lock_guard<std::mutex> submittionLock(GetSubmittionMutex());
		GetCurrentFrameCleanupDelegate().AddLambda(func);
	}

	using CleanupDelegate			= lib::MulticastDelegate<>;

private:

	static void						FlushCurrentFrameReleases();
	static void						FlushAllFramesReleases();

	static std::mutex&		GetSubmittionMutex();
	static CleanupDelegate&			GetCurrentFrameCleanupDelegate();
};

}