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

static lib::SharedPtr<Semaphore>				releaseFrameSemaphore;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CurrentFrameContext ===========================================================================

void CurrentFrameContext::Initialize(Uint32 framesInFlightNum)
{
	priv::framesInFlightNum = framesInFlightNum;
	priv::cleanupDelegates = new CleanupDelegate[framesInFlightNum];

	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Timeline);
	priv::releaseFrameSemaphore = ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("ReleaseFrameSemaphore"), semaphoreDef);
}

void CurrentFrameContext::Shutdown()
{
	priv::releaseFrameSemaphore.reset();

	ReleaseAllResources();

	delete[] priv::cleanupDelegates;
}

void CurrentFrameContext::WaitForFrameRendered(Uint64 frameIdx)
{
	const Bool released = priv::releaseFrameSemaphore->GetRHI().Wait(frameIdx/*, 500000000*/);
	SPT_CHECK_MSG(released, "Failed to release GPU resources");

	FlushFrameReleases(frameIdx);
}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetCurrentFrameCleanupDelegate()
{
	const engn::FrameContext& context = engn::EngineFramesManager::GetRenderingFrame();
	return GetDelegateForFrame(context.GetFrameIdx());
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

void CurrentFrameContext::FlushFrameReleases(Uint64 frameIdx)
{
	SPT_PROFILER_FUNCTION();

	lib::MulticastDelegate<void()> localDelegate = std::move(GetDelegateForFrame(frameIdx));
	js::Launch("Cleanup Frame Resources",
			   [ delegate = std::move(localDelegate) ]() mutable
			   {
				   delegate.ResetAndBroadcast();
			   });
}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetDelegateForFrame(Uint64 frameIdx)
{
	return priv::cleanupDelegates[frameIdx % priv::framesInFlightNum];
}

const lib::SharedPtr<Semaphore>& CurrentFrameContext::GetReleaseFrameSemaphore()
{
	return priv::releaseFrameSemaphore;
}

} // spt::rdr