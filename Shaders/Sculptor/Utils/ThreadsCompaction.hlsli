#ifndef THREADS_COMPACTION_HLSLI
#define THREADS_COMPACTION_HLSLI

#include "Utils/Wave.hlsli"


groupshared uint gs_validThreads[64];
groupshared uint gs_validThreadsNum;

void CompactThreads(inout uint2 localThreadID, inout bool isValidThread)
{
	const uint threadIdx = (localThreadID.x & 7) + (localThreadID.y * 8) + (localThreadID.x > 8u ? 8u * 16u : 0u);
	if (threadIdx < 64)
	{
		gs_validThreads[threadIdx] = 0;
	}

	if(threadIdx == 0)
	{
		gs_validThreadsNum = 0;
	}
	
	GroupMemoryBarrierWithGroupSync();

	const uint validThreadsMask = WaveActiveBallot(isValidThread).x;

	const uint validThreadsNum = countbits(validThreadsMask);
	if(validThreadsNum > 0)
	{
		uint outputThreadIdx = 0;
		if (WaveIsFirstLane())
		{
			InterlockedAdd(gs_validThreadsNum, validThreadsNum, outputThreadIdx);
		}

		outputThreadIdx = WaveReadLaneFirst(outputThreadIdx) + GetCompactedIndex(validThreadsMask, WaveGetLaneIndex());

		const uint subgroupOutputIdx = outputThreadIdx >> 2;
		const uint localSubgroupIdx = outputThreadIdx & 3;

		const uint threadCoordsMask = (localThreadID.x | (localThreadID.y << 4)) << (localSubgroupIdx * 8);
		
		InterlockedOr(gs_validThreads[subgroupOutputIdx], threadCoordsMask);
	}

	GroupMemoryBarrierWithGroupSync();

	if(threadIdx < gs_validThreadsNum)
	{
		const uint subgroupIdx      = threadIdx >> 2;
		const uint localSubgroupIdx = threadIdx & 3;

		const uint threadCoordsMask = gs_validThreads[subgroupIdx] >> (localSubgroupIdx * 8);
		localThreadID.x = threadCoordsMask & 0xF;
		localThreadID.y = (threadCoordsMask >> 4) & 0xF;

		isValidThread = true;
	}
	else
	{
		isValidThread = false;
	}
}


#endif // THREADS_COMPACTION_HLSLI