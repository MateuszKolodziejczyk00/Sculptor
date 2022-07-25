#include "CurrentFrameContext.h"


namespace spt::renderer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{
CurrentFrameContext::CleanupDelegate*					cleanupDelegates;

Uint32													framesInFlightNum = 0;
Uint32													currentFrameIdx = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CurrentFrameContext ===========================================================================

void CurrentFrameContext::Initialize(Uint32 framesInFlightNum)
{
	priv::framesInFlightNum = framesInFlightNum;
	priv::cleanupDelegates = new CleanupDelegate[framesInFlightNum];
}

void CurrentFrameContext::BeginFrame()
{
	SPT_PROFILE_FUNCTION();

	priv::currentFrameIdx = (priv::currentFrameIdx + 1) % priv::framesInFlightNum;
	FlushCurrentFrameReleases();
}

void CurrentFrameContext::EndFrame()
{

}

void CurrentFrameContext::Shutdown()
{
	FlushAllFramesReleases();

	delete[] priv::cleanupDelegates;
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

	for(Uint32 i = 0; i < priv::framesInFlightNum; ++i)
	{
		priv::cleanupDelegates[i].Broadcast();
	}
}

CurrentFrameContext::CleanupDelegate& CurrentFrameContext::GetCurrentFrameCleanupDelegate()
{
	return priv::cleanupDelegates[priv::currentFrameIdx];
}

}