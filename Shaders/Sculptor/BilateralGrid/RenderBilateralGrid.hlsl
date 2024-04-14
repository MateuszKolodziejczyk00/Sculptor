#include "SculptorShader.hlsli"

[[descriptor_set(RenderBilateralGridDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define EPSILON 0.005


#define GRID_DEPTH 64
#define LOCAL_GRID_CELLS_NUM GRID_DEPTH

groupshared uint localGridCellWeights[LOCAL_GRID_CELLS_NUM];


#define GROUP_SIZE_X 32
#define GROUP_SIZE_Y 32

#define GROUP_SIZE (GROUP_SIZE_X * GROUP_SIZE_Y)


float LuminanceToBinIdx(float luminance, float minLogLuminance, float inverseLogLuminanceRange)
{
	// Avoid taking the log of zero
	if (luminance < EPSILON)
	{
		return 0;
	}
	
	const float logLuminance = saturate((log2(luminance) - minLogLuminance) * inverseLogLuminanceRange);
	
	// Map [0, 1] to [1, 63]. The zeroth bin is handled by the epsilon check above.
	return logLuminance * 62.0 + 1.0;
}


float ComputeGridDepthLogLuminance(uint depth, float minLogLuminance, float logLuminanceRange)
{
	const float invSteps = rcp(63.f);
	const float step = logLuminanceRange * invSteps;
	return minLogLuminance + step * depth;
}


void InitializeGridCellWeights(uint localIdx)
{
	if (localIdx < LOCAL_GRID_CELLS_NUM)
	{
		localGridCellWeights[localIdx] = 0;
	}
}


struct BinWeights
{
	uint binIdx;
	uint binWeight;
	uint nextBinWeight;
};


BinWeights ComputeBinWeights(in float binIdx)
{
	const float decimalPart = frac(binIdx);

	BinWeights result;
	result.binIdx = uint(binIdx);
	result.binWeight = decimalPart * 0.01f;
	result.nextBinWeight = 100 - result.binWeight;

	return result;
}


void AddWeightsToGrid(in BinWeights weights)
{
	InterlockedAdd(localGridCellWeights[weights.binIdx], weights.binWeight);
	InterlockedAdd(localGridCellWeights[min(weights.binIdx + 1, GRID_DEPTH - 1)], weights.nextBinWeight);
}


void AddPixelToLocalGrid(float3 linearColor)
{
	const float luminance = Luminance(linearColor);

	const float binIdx = LuminanceToBinIdx(luminance, u_bilateralGridConstants.minLogLuminance, u_bilateralGridConstants.inverseLogLuminanceRange);

	const BinWeights weights = ComputeBinWeights(binIdx);

	AddWeightsToGrid(weights);
}


void WriteGridCell(in uint3 cellCoords)
{
	if(cellCoords.z < GRID_DEPTH)
	{
		const float cellWeight = localGridCellWeights[cellCoords.z] * 0.01f / GROUP_SIZE;
		const float cellLogLuminance = ComputeGridDepthLogLuminance(cellCoords.z, u_bilateralGridConstants.minLogLuminance, u_bilateralGridConstants.logLuminanceRange);
		const float2 cellValue = float2(cellLogLuminance * cellWeight, cellWeight);
		u_bilateralGridTexture[cellCoords] = cellValue;
	}
}


void OutputAverageLogLuminance(in uint2 outputPixel, in uint pixelsNum)
{
	const uint waveIdx = WaveGetLaneIndex();
	const float2 cellWeights = float2(localGridCellWeights[2 * waveIdx], localGridCellWeights[2 * waveIdx + 1]) * 0.01f / pixelsNum;
	const float2 cellLogLuminance = float2(
		ComputeGridDepthLogLuminance(2 * waveIdx, u_bilateralGridConstants.minLogLuminance, u_bilateralGridConstants.logLuminanceRange),
		ComputeGridDepthLogLuminance(2 * waveIdx + 1, u_bilateralGridConstants.minLogLuminance, u_bilateralGridConstants.logLuminanceRange));

	const float averageLogLuminance = WaveActiveSum(dot(cellLogLuminance, cellWeights));

	if(WaveIsFirstLane())
	{
		u_downsampledLogLuminanceTexture[outputPixel] = averageLogLuminance;
	}
}


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void RenderBilateralGridCS(CS_INPUT input)
{
	const uint localIdx = input.localID.y * 32 + input.localID.x;
	InitializeGridCellWeights(localIdx);

	GroupMemoryBarrierWithGroupSync();
	
	const uint2 pixel = input.globalID.xy;

	if(all(pixel < u_bilateralGridConstants.inputTextureResolution))
	{
		const float3 linearColor = u_inputTexture.Load(int3(pixel, 0)).xyz * rcp(GetViewExposure());

		AddPixelToLocalGrid(linearColor);
	}

	GroupMemoryBarrierWithGroupSync();

	const uint3 gridCellCoords = uint3(input.groupID.xy, localIdx);
	WriteGridCell(gridCellCoords);

	if(WaveActiveAnyTrue(localIdx == 0))
	{
		const int pixelsX = min(GROUP_SIZE_X, u_bilateralGridConstants.inputTextureResolution.x - input.groupID.x);
		const int pixelsY = min(GROUP_SIZE_Y, u_bilateralGridConstants.inputTextureResolution.y - input.groupID.y);
		const uint pixelsNum = min(GROUP_SIZE, pixelsX * pixelsY);
		OutputAverageLogLuminance(input.groupID.xy, pixelsNum);
	}
}
