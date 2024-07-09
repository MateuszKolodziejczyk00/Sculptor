#ifndef RAY_BINNING_HLSLI
#define RAY_BINNING_HLSLI

#define RAY_BINNING_TILE_SIZE      16
#define RAY_BINNING_TILE_MASK      (RAY_BINNING_TILE_SIZE - 1)

#define RAY_BINNING_BINS_NUM_1D 4
#define RAY_BINNING_BINS_NUM    (RAY_BINNING_BINS_NUM_1D * RAY_BINNING_BINS_NUM_1D)


uint2 UnpackReorderingData(uint packedReorderedIdx)
{
	return uint2(packedReorderedIdx & 0xF, packedReorderedIdx >> 4);
}

void ExecuteReordering(inout uint2 threadID, in Texture2D<uint> reorderingsTexture)
{
	const uint2 tileCoords = threadID & ~RAY_BINNING_TILE_MASK;
	const uint packedReorderingIdx = reorderingsTexture.Load(uint3(threadID, 0));
	threadID = tileCoords + UnpackReorderingData(packedReorderingIdx);
}

#endif // RAY_BINNING_HLSLI