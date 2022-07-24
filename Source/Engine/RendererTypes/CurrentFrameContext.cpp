#include "CurrentFrameContext.h"


namespace spt::renderer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{
lib::DynamicArray<CurrentFrameContext::CleanupDelegate> cleanupDelegates;

Uint32													currentFrameIdx = 0;

std::mutex												submittionMutex;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CurrentFrameContext ===========================================================================

void CurrentFrameContext::Initialize(Uint32 framesInFlightNum)
{
	priv::cleanupDelegates.resize(static_cast<SizeType>(framesInFlightNum));
}

void CurrentFrameContext::BeginFrame()
{
	SPT_PROFILE_FUNCTION();

	priv::currentFrameIdx = priv::currentFrameIdx % static_cast<Uint32>(priv::cleanupDelegates.size());
	FlushCurrentFrameReleases();
}

void CurrentFrameContext::EndFrame()
{

}

void CurrentFrameContext::Shutdown()
{
	FlushAllFramesReleases();
}

void CurrentFrameContext::FlushCurrentFrameReleases()
{
	SPT_PROFILE_FUNCTION();

	priv::cleanupDelegates[priv::currentFrameIdx].Broadcast();
	priv::cleanupDelegates[priv::currentFrameIdx].Reset();
}

void CurrentFrameContext::FlushAllFramesReleases()
{
	SPT_PROFILE_FUNCTION();

	for (CleanupDelegate& delegate : priv::cleanupDelegates)
	{
		delegate.Broadcast();
	}
}

std::mutex& CurrentFrameContext::GetSubmittionMutex()
{
	return priv::submittionMutex;
}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetCurrentFrameCleanupDelegate()
{
	return priv::cleanupDelegates[priv::currentFrameIdx];
}

}