#include "SculptorShader.hlsli"

[[descriptor_set(CreateVariableRateTextureDS, 0)]]

#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/MortonCode.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};


#define VRT_SIGNAL_VISIBILITY 0
#define VRT_SIGNAL_LIGHTING   1


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 4


#define GS_SAMPLES_X (GROUP_SIZE_X + 2)
#define GS_SAMPLES_Y (GROUP_SIZE_Y + 2)


groupshared float gs_inputSamples[GS_SAMPLES_X][GS_SAMPLES_Y];


void CacheInputSamples(in int2 tileCoords, in int threadIdx)
{
	for(int i = threadIdx; i < (GS_SAMPLES_X * GS_SAMPLES_Y); i += WaveGetLaneCount())
	{
		const int2 sampleCoords = int2(i % GS_SAMPLES_X, i / GS_SAMPLES_X);
		const int2 globalCoords = clamp(tileCoords + sampleCoords - int2(1, 1), int2(0, 0), u_constants.inputResolution);

		const float3 inputSample = u_inputTexture.Load(uint3(globalCoords, 0));

		float signal = 0.0f;
		if(u_constants.signalType == VRT_SIGNAL_VISIBILITY)
		{
			signal = inputSample.x;
		}
		else if(u_constants.signalType == VRT_SIGNAL_LIGHTING)
		{
			signal = Luminance(inputSample);
		}

		gs_inputSamples[sampleCoords.x][sampleCoords.y] = signal;
	}
}


float SobelFilterX(in uint2 localID)
{
	const float f1 = gs_inputSamples[localID.x + 2][localID.y + 0] - gs_inputSamples[localID.x + 0][localID.y + 0];
	const float f2 = gs_inputSamples[localID.x + 2][localID.y + 1] - gs_inputSamples[localID.x + 0][localID.y + 1];
	const float f3 = gs_inputSamples[localID.x + 2][localID.y + 2] - gs_inputSamples[localID.x + 0][localID.y + 2];

	return abs(f1 + 2.0f * f2 + f3);
}


float SobelFilterY(in uint2 localID)
{
	const float f1 = gs_inputSamples[localID.x + 0][localID.y + 2] - gs_inputSamples[localID.x + 0][localID.y + 0];
	const float f2 = gs_inputSamples[localID.x + 1][localID.y + 2] - gs_inputSamples[localID.x + 1][localID.y + 0];
	const float f3 = gs_inputSamples[localID.x + 2][localID.y + 2] - gs_inputSamples[localID.x + 2][localID.y + 0];

	return abs(f1 + 2.0f * f2 + f3);
}


float2 SobelFilter(in uint2 localID)
{
	const float x = SobelFilterX(localID);
	const float y = SobelFilterY(localID);

	return float2(x, y);
}


uint ComputeVariableRateMask(in float2 filter)
{
	uint variableRate = SPT_VARIABLE_RATE_1X1;

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	if(filter.x < u_constants.xThreshold4)
	{
		variableRate |= SPT_VARIABLE_RATE_4X;
	}
	else
#endif // SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	if(filter.x < u_constants.xThreshold2)
	{
		variableRate |= SPT_VARIABLE_RATE_2X;
	}

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	if(filter.y < u_constants.yThreshold4)
	{
		variableRate |= SPT_VARIABLE_RATE_4Y;
	}
	else
#endif // SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	if(filter.y < u_constants.yThreshold2)
	{
		variableRate |= SPT_VARIABLE_RATE_2Y;
	}

	return variableRate;
}


#define FAST_VARIABLE_RATE 0


void WriteCurrentFrameVariableRateData(in RWTexture2D<uint> vrTexture, in uint2 coords, in uint vrMask, in uint frameIdx)
{
#if FAST_VARIABLE_RATE
	const uint curentFrameOffset = (frameIdx & 3) * SPT_VARIABLE_RATE_BITS;
#else
	const uint curentFrameOffset = ((frameIdx >> 1) & 3) * SPT_VARIABLE_RATE_BITS;
#endif // FAST_VARIABLE_RATE
	const uint currentFrameMask = uint(SPT_VARIABLE_RATE_MASK) << curentFrameOffset;

	const uint variableRateData = vrTexture[coords];

	const uint prevVariableRateInSlot = (variableRateData & currentFrameMask) >> curentFrameOffset;

#if FAST_VARIABLE_RATE
	const bool shouldClearSlot = true;
#else
	const bool shouldClearSlot = ((frameIdx & 0x1) == 0);
#endif // FAST_VARIABLE_RATE
	const uint newMaskInSlot = shouldClearSlot ? vrMask : MinVariableRate(vrMask, prevVariableRateInSlot);

	const uint newVRData = (variableRateData & ~currentFrameMask) | (newMaskInSlot << curentFrameOffset);

	vrTexture[coords] = newVRData;
}


void OutputVariableRate2x2(in uint2 localID, in uint localThreadIdx, in uint2 groupID, in float2 filter)
{
	float2 filterValueForOutput = 0.f;
	const uint quadBaseThreadIdx = localThreadIdx & ~3;
	filterValueForOutput = max(filterValueForOutput, WaveReadLaneAt(filter, quadBaseThreadIdx + 0));
	filterValueForOutput = max(filterValueForOutput, WaveReadLaneAt(filter, quadBaseThreadIdx + 1));
	filterValueForOutput = max(filterValueForOutput, WaveReadLaneAt(filter, quadBaseThreadIdx + 2));
	filterValueForOutput = max(filterValueForOutput, WaveReadLaneAt(filter, quadBaseThreadIdx + 3));

	if((localThreadIdx & 3) == 0)
	{
		const uint variableRate = ComputeVariableRateMask(filterValueForOutput);

		const uint2 outputCoords = groupID.xy * uint2(4, 2) + localID.xy / 2;

		WriteCurrentFrameVariableRateData(u_rwVariableRateTexture, outputCoords, variableRate, u_constants.frameIdx);
	}
}


void OutputVariableRate4x4(in uint2 localID, in uint2 groupID, in float2 filter)
{
	const float2 leftPixelFilter  = WaveActiveMax(localID.x < 4  ? filter : 0.f);
	const float2 rightPixelFilter = WaveActiveMax(localID.x >= 4 ? filter : 0.f);

	filter = localID.x < 4 ? leftPixelFilter : rightPixelFilter;
	uint variableRate = ComputeVariableRateMask(filter);

	if(localID.y == 0 && (localID.x & 3) == 0)
	{
		const uint2 outputCoords = groupID.xy * int2(2, 1) + localID.xy / 4;

		WriteCurrentFrameVariableRateData(u_rwVariableRateTexture, outputCoords, variableRate, u_constants.frameIdx);
	}
}


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void CreateVariableRateTextureCS(CS_INPUT input)
{
	const uint threadIdx = input.localID.y * GROUP_SIZE_X + input.localID.x;
	CacheInputSamples(input.groupID.xy * int2(GROUP_SIZE_X, GROUP_SIZE_Y), threadIdx);

	const uint2 locaID = DecodeMorton2D(threadIdx);

	GroupMemoryBarrierWithGroupSync();

	const float2 filter = SobelFilter(input.localID.xy);

	if(u_constants.tileSize == SPT_VARIABLE_RATE_TILE_2x2)
	{
		OutputVariableRate2x2(locaID, threadIdx, input.groupID.xy, filter);
	}
	else if(u_constants.tileSize == SPT_VARIABLE_RATE_TILE_4x4)
	{
		OutputVariableRate4x4(locaID, input.groupID.xy, filter);
	}
}
