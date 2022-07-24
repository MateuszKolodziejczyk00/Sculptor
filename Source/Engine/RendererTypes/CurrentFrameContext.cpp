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
	++priv::currentFrameIdx;
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
	priv::cleanupDelegates[priv::currentFrameIdx].Broadcast();
	priv::cleanupDelegates[priv::currentFrameIdx].Reset();
}

void CurrentFrameContext::FlushAllFramesReleases()
{
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