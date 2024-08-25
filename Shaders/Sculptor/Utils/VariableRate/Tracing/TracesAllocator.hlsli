#ifndef TRACES_ALLOCATOR_HLSLI
#define TRACES_ALLOCATOR_HLSLI

#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/MortonCode.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"


#define TRACES_ALLOCATOR_GROUP_X 16
#define TRACES_ALLOCATOR_GROUP_Y 16


void ReorderLocalIDForAllocating(inout uint2 localID)
{
	const uint threadIdx = localID.x + localID.y * TRACES_ALLOCATOR_GROUP_X;
	localID = DecodeMorton2D(threadIdx);
}

/*
	Format:
		[x, y]
		[z, w]
	Where each component stores max variable rate in 2x2 block
*/
uint4 GetVariableRateIn4x4Block(in uint vrMask, out uint compactedMask4x4, out uint smallBlockIdx)
{
	smallBlockIdx = WaveGetLaneIndex() / 4;
	const uint blockOffset = smallBlockIdx * SPT_VARIABLE_RATE_BITS;

	const uint variableRateMaskShifted = vrMask << blockOffset;

	compactedMask4x4 = WaveActiveBitOr(variableRateMaskShifted);

	if(smallBlockIdx >= 4)
	{
		smallBlockIdx -= 4;
		compactedMask4x4 = compactedMask4x4 >> 16;
	}

	compactedMask4x4 &= ((1 << (4 * SPT_VARIABLE_RATE_BITS)) - 1);

	const uint4 vrMasks4x4 = uint4(
		(compactedMask4x4 >> SPT_VARIABLE_RATE_BITS * 0) & SPT_VARIABLE_RATE_MASK,
		(compactedMask4x4 >> SPT_VARIABLE_RATE_BITS * 1) & SPT_VARIABLE_RATE_MASK,
		(compactedMask4x4 >> SPT_VARIABLE_RATE_BITS * 2) & SPT_VARIABLE_RATE_MASK,
		(compactedMask4x4 >> SPT_VARIABLE_RATE_BITS * 3) & SPT_VARIABLE_RATE_MASK
	);

	return vrMasks4x4;
}


uint ComputeVariableRateMask(in uint requestedVRMask)
{
	// Warning: this MUST be executed by all threads in the wave
	uint compactedMask4x4 = 0;
	uint smallBlockIdx = 0;
	const uint4 vrMasks4x4 = GetVariableRateIn4x4Block(requestedVRMask, OUT compactedMask4x4, OUT smallBlockIdx);

	uint finalMask = requestedVRMask;
#if SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	if(requestedVRMask > SPT_VARIABLE_RATE_2X2)
	{
		// Check if neighboring blocks also want lower rate
		const uint leftBlockIdx = (smallBlockIdx & ~1); // morton order
		const uint horizontalMask = vrMasks4x4[leftBlockIdx] & vrMasks4x4[leftBlockIdx + 1];
		const bool supports4X = (horizontalMask & SPT_VARIABLE_RATE_4X) > 0;

		const uint upperBlockIdx = (smallBlockIdx & ~2); // morton order
		const uint verticalMask = vrMasks4x4[upperBlockIdx] & vrMasks4x4[upperBlockIdx + 2];
		const bool supports4Y = (verticalMask & SPT_VARIABLE_RATE_4Y) > 0;

		const uint all4x4 = SPT_VARIABLE_RATE_4X4 
						  | (SPT_VARIABLE_RATE_4X4 << (SPT_VARIABLE_RATE_BITS))
						  | (SPT_VARIABLE_RATE_4X4 << (SPT_VARIABLE_RATE_BITS * 2))
						  | (SPT_VARIABLE_RATE_4X4 << (SPT_VARIABLE_RATE_BITS * 3));

		const bool supports4X4 = (compactedMask4x4 == all4x4);

		finalMask = ClampTo2x2VariableRate(requestedVRMask);
		if (requestedVRMask == SPT_VARIABLE_RATE_4X4 && supports4X4)
		{
			finalMask |= SPT_VARIABLE_RATE_4X4;
			finalMask &= ~SPT_VARIABLE_RATE_2X2;
		}
		else
		{
			if((requestedVRMask & SPT_VARIABLE_RATE_4X && supports4X) > 0)
			{
				finalMask |= SPT_VARIABLE_RATE_4X;
				finalMask &= ~SPT_VARIABLE_RATE_2X;
			}
			else if((requestedVRMask & SPT_VARIABLE_RATE_4Y) > 0)
			{
				// 4X has higher priority than 4Y
				// because of that, we need to check if 4X is supported and allow 4Y only if 4X is not supported for all rows
				const uint upperRowMask = vrMasks4x4[0] & vrMasks4x4[1];
				const uint lowerRowMask = vrMasks4x4[2] & vrMasks4x4[3];
				const bool anyRowSupports4X = ((upperRowMask | lowerRowMask) & SPT_VARIABLE_RATE_4X) > 0;

				if(supports4Y && !anyRowSupports4X)
				{
					finalMask |= SPT_VARIABLE_RATE_4Y;
					finalMask &= ~SPT_VARIABLE_RATE_2Y;
				}
			}
		}
	}
#endif // SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2

	return finalMask;
}


groupshared uint gs_tracesToAllocateNum;
groupshared uint gs_tracesAllocationOffset;


struct TracesAllocator
{
	static TracesAllocator Create(in RWStructuredBuffer<EncodedRayTraceCommand> tracesCommands, in RWStructuredBuffer<uint> inTracesCommandsNum, in RWTexture2D<uint> inVariableRateBlocksTexture)
	{
		TracesAllocator allocator;
		allocator.m_tracesCommands            = tracesCommands;
		allocator.m_tracesCommandsNum         = inTracesCommandsNum;
		allocator.m_variableRateBlocksTexture = inVariableRateBlocksTexture;
		allocator.m_noise                     = -1.f;
		return allocator;
	}

	void SetNoiseValue(in float inValue)
	{
		m_noise = inValue;
	}

#if OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
	void SetTracesNumBuffers(in RWStructuredBuffer<uint> inTracesNum, in RWStructuredBuffer<uint> inTracesDispatchGroupsNum)
	{
		m_tracesNum               = inTracesNum;
		m_tracesDispatchGroupsNum = inTracesDispatchGroupsNum;
	}
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM

	void AllocateTraces(in uint2 groupID, in uint2 localID, in uint variableRateMask, in uint traceIdx, in bool maskOutOutput)
	{
		if(all(groupID + localID) == 0)
		{
			m_tracesCommandsNum[1] = 1;
			m_tracesCommandsNum[2] = 1;
		}

		if(all(localID == 0))
		{
			gs_tracesToAllocateNum = 0;
		}

		GroupMemoryBarrierWithGroupSync();

		variableRateMask = ComputeVariableRateMask(variableRateMask);

		const uint2 vrTileSize = GetVariableRateTileSize(variableRateMask);

		const uint2 globalID = groupID * (TRACES_ALLOCATOR_GROUP_X, TRACES_ALLOCATOR_GROUP_Y) + localID;

		{
			// Allocate individual traces
			const uint2 vrTileMask = vrTileSize - 1;

			const bool wantsToAllocateTrace = all((localID & vrTileMask) == 0) && !maskOutOutput;

			const uint2 wantsTraceBallot = WaveActiveBallot(wantsToAllocateTrace).xy;
			const uint tracesToAllocate = countbits(wantsTraceBallot.x) + countbits(wantsTraceBallot.y);

			uint traceOutputIdx = 0;
			if (WaveIsFirstLane())
			{
				InterlockedAdd(gs_tracesToAllocateNum, tracesToAllocate, OUT traceOutputIdx);
			}

			GroupMemoryBarrierWithGroupSync();

			if (WaveIsFirstLane() && traceOutputIdx == 0 && tracesToAllocate > 0)
			{
				uint allocationOffset = 0;
				const uint tracesToAllocateNum = gs_tracesToAllocateNum;
				InterlockedAdd(m_tracesCommandsNum[0], tracesToAllocateNum, OUT allocationOffset);
				gs_tracesAllocationOffset = allocationOffset;

#if OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
				uint tracesNum = 0;
				InterlockedAdd(m_tracesNum[0], tracesToAllocateNum, OUT tracesNum);
				InterlockedMax(m_tracesDispatchGroupsNum[0], (tracesNum + tracesToAllocateNum + 31) / 32);
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
			}

			GroupMemoryBarrierWithGroupSync();

			if (WaveIsFirstLane())
			{
				traceOutputIdx += gs_tracesAllocationOffset;
			}

			traceOutputIdx = WaveReadLaneFirst(traceOutputIdx) + GetCompactedIndex(wantsTraceBallot, WaveGetLaneIndex());

			uint2 localOffset = 0u;
			if(m_noise >= 0.f)
			{
				const uint tileArea = vrTileSize.x * vrTileSize.y;
				const uint pixelIdx = frac(m_noise + SPT_GOLDEN_RATIO * (traceIdx & 31)) * tileArea;
				localOffset = uint2(pixelIdx % vrTileSize.x, pixelIdx / vrTileSize.x);
			}

			if(wantsToAllocateTrace)
			{
				RayTraceCommand traceCommand;
				traceCommand.blockCoords      = globalID;
				traceCommand.localOffset      = localOffset;
				traceCommand.variableRateMask = variableRateMask;

				const EncodedRayTraceCommand encodedTraceCommand = EncodeTraceCommand(traceCommand);
				m_tracesCommands[traceOutputIdx] = encodedTraceCommand;
			}

			if(!maskOutOutput)
			{
				m_variableRateBlocksTexture[globalID] = PackVRBlockInfo(localOffset, variableRateMask);
			}
		}
	}

	RWStructuredBuffer<EncodedRayTraceCommand> m_tracesCommands;
	RWStructuredBuffer<uint>                   m_tracesCommandsNum;
	RWTexture2D<uint>                          m_variableRateBlocksTexture;

	float m_noise;

#if OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
	RWStructuredBuffer<uint>                   m_tracesNum;
	RWStructuredBuffer<uint>                   m_tracesDispatchGroupsNum;
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
};


#endif // TRACES_ALLOCATOR_HLSLI