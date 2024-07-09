#include "SculptorShader.hlsli"

[[descriptor_set(RayBinningDS, 0)]]

#include "Utils/RayBinning/RayBinning.hlsli"


groupshared int gs_bins[RAY_BINNING_BINS_NUM];


void InitializeBinSizes(uint threadIdx)
{
	if(threadIdx < RAY_BINNING_BINS_NUM)
	{
		gs_bins[threadIdx] = 0;
	}
}


uint ComputeBinIdxForRay(in float2 rayDirOctahedron)
{
	const uint2 binCoords = uint2(rayDirOctahedron * RAY_BINNING_BINS_NUM_1D);
	const uint binIdx = binCoords.x | (binCoords.y << 2);
	return binIdx;
}


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


[numthreads(RAY_BINNING_TILE_SIZE, RAY_BINNING_TILE_SIZE, 1)]
void RayBinningCS(CS_INPUT input)
{
	const uint localThreadIdx = input.localID.y * RAY_BINNING_TILE_SIZE + input.localID.x;
	InitializeBinSizes(localThreadIdx);

	int idxInBin = -1;
	int binIdx   = -1;

	GroupMemoryBarrierWithGroupSync();

	if(all(input.globalID.xy < u_constants.resolution))
	{
		const float2 rayDir = u_rayDirections.Load(input.globalID);

		binIdx = ComputeBinIdxForRay(rayDir);
		InterlockedAdd(gs_bins[binIdx], 1, OUT idxInBin);
	}

	GroupMemoryBarrierWithGroupSync();

	if(WaveActiveAnyTrue(localThreadIdx == 0))
	{
		uint binIdx = WaveGetLaneIndex();
		if(binIdx < RAY_BINNING_BINS_NUM)
		{
			const uint binSize = gs_bins[binIdx];
			const uint binOffset = WavePrefixSum(binSize);
			gs_bins[binIdx] = binOffset;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if(idxInBin >= 0)
	{
		const uint tileSizeX = min(u_constants.resolution.x - input.groupID.x * RAY_BINNING_TILE_SIZE, RAY_BINNING_TILE_SIZE);

		const uint outputPixelIdx     = gs_bins[binIdx] + idxInBin;

		uint2 outputPixelInTile = uint2(outputPixelIdx % min(8, tileSizeX), outputPixelIdx / 8);
		if(outputPixelInTile.y > 16)
		{
			outputPixelInTile.y -= 16;
			outputPixelInTile.x += 8;
		}

		const uint packedReorderedIdx =  input.localID.y << 4 | input.localID.x;

		const uint2 outputPixel = input.groupID.xy * RAY_BINNING_TILE_SIZE + outputPixelInTile;

		u_rwReorderingsTexture[outputPixel] = packedReorderedIdx;
	}
}
