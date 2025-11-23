#ifndef TRACES_ALLOCATOR_HLSLI
#define TRACES_ALLOCATOR_HLSLI

#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/MortonCode.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"


#define TRACES_ALLOCATOR_GROUP_X 32
#define TRACES_ALLOCATOR_GROUP_Y 32


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


CompactedMask4x4 GetVariableRateIn4x4Block(in uint vrMask, out uint outTileIdxIn4x4Quad)
{
	outTileIdxIn4x4Quad = WaveGetLaneIndex() / 4;
	const uint blockOffset = outTileIdxIn4x4Quad * SPT_VARIABLE_RATE_BITS;

	const uint variableRateMaskShifted = vrMask << blockOffset;

	uint compactedMask = WaveActiveBitOr(variableRateMaskShifted);

	if(outTileIdxIn4x4Quad >= 4)
	{
		// handle right 4x4 quad
		outTileIdxIn4x4Quad -= 4;
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
	uint outTileIdxIn4x4Quad = 0;
	const CompactedMask4x4 compactedMask4x4 = GetVariableRateIn4x4Block(requestedVRMask, OUT outTileIdxIn4x4Quad);

	uint finalMask = requestedVRMask;
	if(requestedVRMask > SPT_VARIABLE_RATE_2X2)
	{
		const uint rowIdx = outTileIdxIn4x4Quad >> 1u;
		const uint colIdx = outTileIdxIn4x4Quad & 1u;

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
		return allocator;
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

			const uint2 allocatingLocalID = (localID & ~vrTileMask);
			const uint allocatingLaneID = EncodeMorton2(allocatingLocalID) & (WaveGetLaneCount() - 1u);
			const bool wantsToAllocateTrace = allocatingLaneID == WaveGetLaneIndex() && !maskOutOutput;

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
				InterlockedMax(m_tracesDispatchGroupsNum[0], (tracesNum + tracesToAllocateNum + 63) / 64);
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
			}

			GroupMemoryBarrierWithGroupSync();

			if (WaveIsFirstLane())
			{
				traceOutputIdx += gs_tracesAllocationOffset;
			}

			traceOutputIdx = WaveReadLaneFirst(traceOutputIdx) + GetCompactedIndex(wantsTraceBallot, WaveGetLaneIndex());

			const uint tileArea = vrTileSize.x * vrTileSize.y;
			uint2 localOffset;
			if(tileArea > 4u)
			{
				const uint2 tiles2x2      = vrTileSize >> 1u;
				const uint tiles2x2Num    = tiles2x2.x * tiles2x2.y;
				const uint logTiles2x2Num = firstbitlow(tiles2x2Num);
				const uint tiles2x2Mask   = tiles2x2Num - 1u;

				const uint idx2x2 = traceIdx & tiles2x2Mask;
				const uint idxIn2x2 = (traceIdx >> logTiles2x2Num) & 3u;

				const uint logTiles2x2X = firstbitlow(tiles2x2.x);
				localOffset = uint2(idx2x2 & (tiles2x2.x - 1u), (idx2x2 >> logTiles2x2X)) * 2u // 2x2 tile offset
                            + uint2(idxIn2x2 & 1u, (idxIn2x2 >> 1u) & 1u); // pixel offset in 2x2 tile
			}
			else
			{
				const uint pixelIdx = traceIdx & (tileArea - 1u);
				const uint logTileSizeX = firstbitlow(vrTileSize.x);
				localOffset = uint2(pixelIdx & (vrTileSize.x - 1u), (pixelIdx >> logTileSizeX));
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

#if OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
	RWStructuredBuffer<uint>                   m_tracesNum;
	RWStructuredBuffer<uint>                   m_tracesDispatchGroupsNum;
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
};


#endif // TRACES_ALLOCATOR_HLSLI