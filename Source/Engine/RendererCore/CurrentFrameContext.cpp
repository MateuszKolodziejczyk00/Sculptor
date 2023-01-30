#include "CurrentFrameContext.h"
#include "ResourcesManager.h"
#include "Types/Semaphore.h"
#include "Renderer.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{
static CurrentFrameContext::CleanupDelegate*					cleanupDelegates;

static Uint32													framesInFlightNum = 0;
static Uint32													currentCleanupDelegateIdx = 0;
static Uint64													currentFrameIdx = 0;

static lib::SharedPtr<Semaphore>								releaseFrameSemaphore;

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

void CurrentFrameContext::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	++priv::currentFrameIdx;

	const Bool shouldWaitForFrameFinished = priv::currentFrameIdx > priv::framesInFlightNum;

	if (shouldWaitForFrameFinished)
	{
		const Uint64 releasedFrameIdx = priv::currentFrameIdx - static_cast<Uint64>(priv::framesInFlightNum);
		const Bool released = priv::releaseFrameSemaphore->GetRHI().Wait(releasedFrameIdx);
		SPT_CHECK(released);
	}

	priv::currentCleanupDelegateIdx = (priv::currentCleanupDelegateIdx + 1) % priv::framesInFlightNum;

	// flush releases only if we were wating for finishing frame. Otherwise we may remove resources that are in use (for example resources used for loading or initialization)
	if (shouldWaitForFrameFinished)
	{
		FlushCurrentFrameReleases();
	}
}

void CurrentFrameContext::EndFrame()
{

}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetCurrentFrameCleanupDelegate()
{
	return priv::cleanupDelegates[priv::currentCleanupDelegateIdx];
}

void CurrentFrameContext::ReleaseAllResources()
{
	SPT_PROFILER_FUNCTION();

	for (Uint32 i = 0; i < priv::framesInFlightNum; ++i)
	{
		priv::cleanupDelegates[i].Broadcast();
		priv::cleanupDelegates[i].Reset();
	}
}

Uint64 CurrentFrameContext::GetCurrentFrameIdx()
{
	return priv::currentFrameIdx;
}

const lib::SharedPtr<Semaphore>& CurrentFrameContext::GetReleaseFrameSemaphore()
{
	return priv::releaseFrameSemaphore;
}

void CurrentFrameContext::FlushCurrentFrameReleases()
{
	SPT_PROFILER_FUNCTION();

	priv::cleanupDelegates[priv::currentCleanupDelegateIdx].Broadcast();
	priv::cleanupDelegates[priv::currentCleanupDelegateIdx].Reset();
}

}