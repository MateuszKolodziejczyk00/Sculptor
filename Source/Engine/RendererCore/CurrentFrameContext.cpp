#include "CurrentFrameContext.h"
#include "ResourcesManager.h"
#include "Types/Semaphore.h"
#include "Renderer.h"
#include "EngineFrame.h"
#include "JobSystem.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{
static CurrentFrameContext::CleanupDelegate*	cleanupDelegates;

static Uint32									framesInFlightNum = 0;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CurrentFrameContext ===========================================================================

void CurrentFrameContext::Initialize(Uint32 framesInFlightNum)
{
	priv::framesInFlightNum = framesInFlightNum;
	priv::cleanupDelegates = new CleanupDelegate[priv::framesInFlightNum];
}

void CurrentFrameContext::Shutdown()
{
	ReleaseAllResources();

	delete[] priv::cleanupDelegates;
}

void CurrentFrameContext::FlushFrameReleases(Uint64 frameIdx)
{
	SPT_PROFILER_FUNCTION();

	lib::MulticastDelegate<void()> localDelegate = std::move(GetDelegateForFrame(frameIdx));
	js::Launch("Cleanup Frame Resources",
			   [ delegate = std::move(localDelegate) ]() mutable
			   {
				   delegate.ResetAndBroadcast();
			   },
			   js::JobDef().SetPriority(js::EJobPriority::High));
}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetCurrentFrameCleanupDelegate()
{
	const Uint64 frameIdx = rdr::Renderer::GetCurrentFrameIdx();
	return GetDelegateForFrame(frameIdx);
}

void CurrentFrameContext::ReleaseAllResources()
{
	SPT_PROFILER_FUNCTION();

	for (SizeType delegateIdx = 0; delegateIdx < priv::framesInFlightNum; ++delegateIdx)
	{
		priv::cleanupDelegates[delegateIdx].ResetAndBroadcast();
	}
	
	CleanupDelegate& currentFrameDelegate = GetCurrentFrameCleanupDelegate();

	while (currentFrameDelegate.IsBound())
	{
		currentFrameDelegate.ResetAndBroadcast();
	}
}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetDelegateForFrame(Uint64 frameIdx)
{
	return priv::cleanupDelegates[frameIdx % priv::framesInFlightNum];
}

} // spt::rdr