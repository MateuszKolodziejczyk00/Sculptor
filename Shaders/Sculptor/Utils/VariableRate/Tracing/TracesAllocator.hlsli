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


struct CompactedMask4x4
{
	static CompactedMask4x4 Create(uint inMask)
	{
		CompactedMask4x4 instance;
		instance.mask = inMask;
		return instance;
	}

	uint operator[](in uint idx)
	{
		return (mask >> (SPT_VARIABLE_RATE_BITS * idx)) & SPT_VARIABLE_RATE_MASK;
	}

	// 4 morton ordered 2x2 tiles
	uint mask;
};


CompactedMask4x4 GetVariableRateIn4x4Block(in uint vrMask, out uint tileIdxInQuad)
{
	tileIdxInQuad = WaveGetLaneIndex() / 4;
	const uint blockOffset = tileIdxInQuad * SPT_VARIABLE_RATE_BITS;

	const uint variableRateMaskShifted = vrMask << blockOffset;

	uint compactedMask = WaveActiveBitOr(variableRateMaskShifted);

	if(tileIdxInQuad >= 4)
	{
		// handle right 4x4 quad
		tileIdxInQuad -= 4;
		compactedMask = compactedMask >> (4 * SPT_VARIABLE_RATE_BITS);
	}

	// mask right quad out for the left quad
	compactedMask &= ((1u << (4 * SPT_VARIABLE_RATE_BITS)) - 1);

	return CompactedMask4x4::Create(compactedMask);
}


uint ComputeVariableRateMask(in uint requestedVRMask)
{
	// Early our if current mask is lower than 2x2. In that case, this thread doesn't care about neighbors
#if SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	if(requestedVRMask <= SPT_VARIABLE_RATE_2X2)
#endif // SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	{
		return requestedVRMask;
	}

#if SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	uint tileIdxInQuad = 0;
	const CompactedMask4x4 compactedMask4x4 = GetVariableRateIn4x4Block(requestedVRMask, OUT tileIdxInQuad);

	uint finalMask = requestedVRMask;
	if(requestedVRMask > SPT_VARIABLE_RATE_2X2)
	{
		const uint rowIdx = tileIdxInQuad >> 1u;
		const uint colIdx = tileIdxInQuad & 1u;

		// First row uses first 8 bits, second row uses next 8 bits
		// First column uses bits 0-3 and 8-11, second column uses bits 4-7 and 12-15

		// offset to get next pixel in column/row in 2x2 block
		const uint bitsOffsetPerRow = SPT_VARIABLE_RATE_BITS * 2u;
		const uint bitsOffsetPerCol = SPT_VARIABLE_RATE_BITS;

		// offsets for current lane
		const uint bitShiftForCurrentRow = rowIdx * bitsOffsetPerRow;
		const uint bitShiftForCurrentCol = colIdx * bitsOffsetPerCol;

		// masks for 1st column and 1st row (need to be shifted for current pixel if it's in different column/row)
		const uint firstRow4XRequiredMask = SPT_VARIABLE_RATE_4X | (SPT_VARIABLE_RATE_4X << bitsOffsetPerCol);
		const uint firstCol4YRequiredMask = SPT_VARIABLE_RATE_4Y | (SPT_VARIABLE_RATE_4Y << bitsOffsetPerRow);

		const bool supports4X = HasAllBits(compactedMask4x4.mask, firstRow4XRequiredMask << bitShiftForCurrentRow);
		const bool supports4Y = HasAllBits(compactedMask4x4.mask, firstCol4YRequiredMask << bitShiftForCurrentCol);

		const uint all4x4 = SPT_VARIABLE_RATE_4X4 
						  | (SPT_VARIABLE_RATE_4X4 << (SPT_VARIABLE_RATE_BITS))
						  | (SPT_VARIABLE_RATE_4X4 << (SPT_VARIABLE_RATE_BITS * 2))
						  | (SPT_VARIABLE_RATE_4X4 << (SPT_VARIABLE_RATE_BITS * 3));

		const bool supports4X4 = (compactedMask4x4.mask == all4x4);

		finalMask = ClampTo2x2VariableRate(requestedVRMask);

		// First, check 4x4
		if (supports4X4)
		{
			finalMask |= SPT_VARIABLE_RATE_4X4;
			finalMask &= ~SPT_VARIABLE_RATE_2X2;
		}
		else if(supports4X) // 4X has higher priority than 4Y
		{
			// 4X2Y
				finalMask |= SPT_VARIABLE_RATE_4X;
				finalMask &= ~SPT_VARIABLE_RATE_2X;
			}
		else if(supports4Y)
			{
				// 4X has higher priority than 4Y
				// because of that, we need to check if 4X is supported and allow 4Y only if 4X is not supported for all rows
				const bool anyRowSupports4X = HasAllBits(compactedMask4x4.mask, firstRow4XRequiredMask) || HasAllBits(compactedMask4x4.mask, firstRow4XRequiredMask << bitsOffsetPerRow);

			if(!anyRowSupports4X)
				{
					finalMask |= SPT_VARIABLE_RATE_4Y;
					finalMask &= ~SPT_VARIABLE_RATE_2Y;
				}
			}
		}

	return finalMask;
#endif // SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
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
				const uint pixelIdx = frac(m_noise + SPT_GOLDEN_RATIO * (traceIdx & 63)) * tileArea;
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