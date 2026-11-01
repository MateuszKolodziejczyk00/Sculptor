#ifndef VARIABLE_RATE_BUILDER_HLSLI
#define VARIABLE_RATE_BUILDER_HLSLI

[[shader_params(CreateVariableRateTextureConstants, PARAMS_CREATE_VARIABLE_RATE_TEXTURE)]]

#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/MortonCode.hlsli"

#ifndef VR_BUILDER_SINGLE_LANE_PER_QUAD
#error "VR_BUILDER_SINGLE_LANE_PER_QUAD must be defined"
#endif // VR_BUILDER_SINGLE_LANE_PER_QUAD

#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

#define GS_SAMPLES_X (GROUP_SIZE_X + 2)
#define GS_SAMPLES_Y (GROUP_SIZE_Y + 2)


#if !VR_BUILDER_SINGLE_LANE_PER_QUAD
groupshared float gs_inputSamples[GS_SAMPLES_X][GS_SAMPLES_Y];
groupshared float gs_depth[GROUP_SIZE_X][GROUP_SIZE_Y];


void CacheInputSamples(in SRVTexture2D<float> input, in int2 tileCoords, in int threadIdx)
{
	for(int i = threadIdx; i < (GS_SAMPLES_X * GS_SAMPLES_Y); i += WaveGetLaneCount())
	{
		const int2 sampleCoords = int2(i % GS_SAMPLES_X, i / GS_SAMPLES_X);
		const int2 globalCoords = clamp(tileCoords + sampleCoords - int2(1, 1), int2(0, 0), PARAMS_CREATE_VARIABLE_RATE_TEXTURE->inputResolution);

		const float signal = input.Load(uint3(globalCoords, 0));

		gs_inputSamples[sampleCoords.x][sampleCoords.y] = signal;
	}
}


void CacheDepthSamples(in SRVTexture2D<float> depth, in int2 tileCoords, in int threadIdx)
{
	for(int i = threadIdx; i < (GROUP_SIZE_X * GROUP_SIZE_Y); i += WaveGetLaneCount())
	{
		const int2 sampleCoords = int2(i % GROUP_SIZE_X, i / GROUP_SIZE_Y);
		const int2 globalCoords = clamp(tileCoords + sampleCoords, int2(0, 0), PARAMS_CREATE_VARIABLE_RATE_TEXTURE->inputResolution);

		gs_depth[sampleCoords.x][sampleCoords.y] = depth.Load(uint3(globalCoords, 0));
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
#endif // !VR_BUILDER_SINGLE_LANE_PER_QUAD

float2 ApplyDepthBasedEdgeFilter(in uint2 localID, in float2 edgeFilter)
{
	const float leftDepth  = gs_depth[(localID.x & ~ 1u) + 0u][localID.y];
	const float rightDepth = gs_depth[(localID.x & ~ 1u) + 1u][localID.y];

	const float topDepth   = gs_depth[localID.x][(localID.y & ~ 1u) + 0u];
	const float bottomDepth = gs_depth[localID.x][(localID.y & ~ 1u) + 1u];

	const bool leftInvalid   = leftDepth   <= 0.f;
	const bool rightInvalid  = rightDepth  <= 0.f;
	const bool topInvalid    = topDepth    <= 0.f;
	const bool bottomInvalid = bottomDepth <= 0.f;

	if (leftInvalid ^ rightInvalid)
	{
		edgeFilter.x = FLOAT_MAX;
	}
	if (topInvalid ^ bottomInvalid)
	{
		edgeFilter.y = FLOAT_MAX;
	}

	return edgeFilter;
}

#define FAST_VARIABLE_RATE 1


void WriteCurrentFrameVariableRateData(in UAVTexture2D<uint> vrTexture, in uint2 coords, in uint vrMask, in uint frameIdx)
{
	const uint logFramesPerSlot  = PARAMS_CREATE_VARIABLE_RATE_TEXTURE->logFramesNumPerSlot;
	const uint curentFrameOffset = ((frameIdx >> logFramesPerSlot) & 3) * SPT_VARIABLE_RATE_BITS;
	const uint currentFrameMask  = uint(SPT_VARIABLE_RATE_MASK) << curentFrameOffset;

	const uint variableRateData = vrTexture[coords];

	const uint prevVariableRateInSlot = (variableRateData & currentFrameMask) >> curentFrameOffset;

	const bool shouldClearSlot = logFramesPerSlot > 0 ? firstbitlow(frameIdx) >= logFramesPerSlot : true;

	const uint newMaskInSlot = shouldClearSlot ? vrMask : MinVariableRate(vrMask, prevVariableRateInSlot);

	const uint newVRData = (variableRateData & ~currentFrameMask) | (newMaskInSlot << curentFrameOffset);

	vrTexture[coords] = newVRData;
}


struct VariableRateProcessor
{
	static VariableRateProcessor Create(in uint2 groupID, in uint localThreadIdx)
	{
		VariableRateProcessor processor;
#if VR_BUILDER_SINGLE_LANE_PER_QUAD
		processor.m_globalCoords = (groupID * uint2(GROUP_SIZE_X, GROUP_SIZE_Y) + uint2(localThreadIdx % GROUP_SIZE_X, localThreadIdx / GROUP_SIZE_X)) * 2u;
#else
		processor.m_groupID        = groupID;
		processor.m_localID        = DecodeMorton2D(localThreadIdx);
		processor.m_localThreadIdx = localThreadIdx;
		processor.m_globalCoords   = min(groupID * uint2(GROUP_SIZE_X, GROUP_SIZE_Y) + processor.m_localID, PARAMS_CREATE_VARIABLE_RATE_TEXTURE->inputResolution - 1);
#endif // VR_BUILDER_SINGLE_LANE_PER_QUAD
		return processor;
	}

	uint2 GetCoords()
	{
		return m_globalCoords;
	}

#if !VR_BUILDER_SINGLE_LANE_PER_QUAD
	float QuadMax(in float value)
	{
		float result = value;
		const uint quadBaseThreadIdx = WaveGetLaneIndex() & ~3;
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 0));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 3));
		return result;
	}

	float2 QuadMax(in float2 value)
	{
		float2 result = value;
		const uint quadBaseThreadIdx = WaveGetLaneIndex() & ~3;
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 0));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 3));
		return result;
	}

	float QuadMin(in float value)
	{
		float result = value;
		const uint quadBaseThreadIdx = WaveGetLaneIndex() & ~3;
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 0));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 3));
		return result;
	}

	float2 QuadMin(in float2 value)
	{
		float2 result = value;
		const uint quadBaseThreadIdx = WaveGetLaneIndex() & ~3;
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 0));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 3));
		return result;
	}

	float DDX_QuadMax(in float value)
	{
		const uint quadBaseThreadIdx = WaveGetLaneIndex() & ~3;
		float ddx = WaveReadLaneAt(value, quadBaseThreadIdx + 1) - WaveReadLaneAt(value, quadBaseThreadIdx);
		ddx = max(ddx, WaveReadLaneAt(value, quadBaseThreadIdx + 3) - WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		return ddx;
	}

	float DDY_QuadMax(in float value)
	{
		const uint quadBaseThreadIdx = WaveGetLaneIndex() & ~3;
		float ddy = WaveReadLaneAt(value, quadBaseThreadIdx + 2) - WaveReadLaneAt(value, quadBaseThreadIdx);
		ddy = max(ddy, WaveReadLaneAt(value, quadBaseThreadIdx + 3) - WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		return ddy;
	}

	uint ComputeEdgeBasedVariableRateMask(in float2 edgeFilter)
	{
		uint variableRate = SPT_VARIABLE_RATE_1X1;

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.x < PARAMS_CREATE_VARIABLE_RATE_TEXTURE->xThreshold4)
		{
			variableRate |= SPT_VARIABLE_RATE_4X;
		}
		else
#endif // SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.x < PARAMS_CREATE_VARIABLE_RATE_TEXTURE->xThreshold2)
		{
			variableRate |= SPT_VARIABLE_RATE_2X;
		}

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.y < PARAMS_CREATE_VARIABLE_RATE_TEXTURE->yThreshold4)
		{
			variableRate |= SPT_VARIABLE_RATE_4Y;
		}
		else
#endif // SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.y < PARAMS_CREATE_VARIABLE_RATE_TEXTURE->yThreshold2)
		{
			variableRate |= SPT_VARIABLE_RATE_2Y;
		}

		return variableRate;
	}

	float2 ComputeEdgeFilter(in SRVTexture2D<float> texture, in SRVTexture2D<float> depth)
	{
		const uint2 tileCoords = m_groupID * uint2(GROUP_SIZE_X, GROUP_SIZE_Y);
		CacheInputSamples(texture, tileCoords, m_localThreadIdx);
		CacheDepthSamples(depth, tileCoords, m_localThreadIdx);

		GroupMemoryBarrierWithGroupSync();

		float2 edgeFilter = SobelFilter(m_localID);

		edgeFilter = ApplyDepthBasedEdgeFilter(m_localID, edgeFilter);

		edgeFilter = QuadMax(edgeFilter);

		return edgeFilter;
	}
#endif // !VR_BUILDER_SINGLE_LANE_PER_QUAD

	uint2 m_globalCoords;
#if !VR_BUILDER_SINGLE_LANE_PER_QUAD
	uint2 m_groupID;
	uint2 m_localID;
	uint  m_localThreadIdx;
#endif // !VR_BUILDER_SINGLE_LANE_PER_QUAD
};


interface IVariableRateBuilderCallback
{
	uint ComputeVariableRateMask(in VariableRateProcessor processor);
};


struct VariableRateBuilder
{
	static void BuildVariableRateTexture<TCallback : IVariableRateBuilderCallback>(in uint2 groupID, in int localThreadIdx, in TCallback callback)
	{
		VariableRateProcessor processor = VariableRateProcessor::Create(groupID, localThreadIdx);

		const uint variableRate = callback.ComputeVariableRateMask(processor);

#if VR_BUILDER_SINGLE_LANE_PER_QUAD
#if VR_USE_LARGE_TILE
#error // not implemented
#else
		WriteCurrentFrameVariableRateData(PARAMS_CREATE_VARIABLE_RATE_TEXTURE->rwVariableRateTexture, processor.GetCoords() / 2u, variableRate, PARAMS_CREATE_VARIABLE_RATE_TEXTURE->frameIdx);
#endif // VR_USE_LARGE_TILE
#else
#if VR_USE_LARGE_TILE
		// not exactly current, for example if half of the lanes have 2x1 and half have 1x2. Ideally it should be reduced to 1x1
		// but in practice using any of these is ok as they are based on same noise thresholds
		const uint largeTileVariableRate = WaveActiveMin(variableRate);
		const uint2 outputCoords = groupID.xy;
		if (localThreadIdx == 0u && all(outputCoords < PARAMS_CREATE_VARIABLE_RATE_TEXTURE->outputResolution))
		{
			WriteCurrentFrameVariableRateData(PARAMS_CREATE_VARIABLE_RATE_TEXTURE->rwVariableRateTexture, outputCoords, largeTileVariableRate, PARAMS_CREATE_VARIABLE_RATE_TEXTURE->frameIdx);
		}
#else
		const uint2 outputCoords = groupID.xy * uint2(4, 4) + processor.m_localID / 2;
		if ((localThreadIdx & 3) == 0 && all(outputCoords < PARAMS_CREATE_VARIABLE_RATE_TEXTURE->outputResolution))
		{
			WriteCurrentFrameVariableRateData(PARAMS_CREATE_VARIABLE_RATE_TEXTURE->rwVariableRateTexture, outputCoords, variableRate, PARAMS_CREATE_VARIABLE_RATE_TEXTURE->frameIdx);
		}
#endif
#endif // VR_BUILDER_SINGLE_LANE_PER_QUAD
	}
};


#if !VR_BUILDER_SINGLE_LANE_PER_QUAD
struct GenericVariableRateCallback : IVariableRateBuilderCallback
{
	uint ComputeVariableRateMask(in VariableRateProcessor processor)
	{
		const float2 edgeFilter = processor.ComputeEdgeFilter(PARAMS_CREATE_VARIABLE_RATE_TEXTURE->inputTexture, PARAMS_CREATE_VARIABLE_RATE_TEXTURE->depthTexture);
		return processor.ComputeEdgeBasedVariableRateMask(edgeFilter);
	}
};
#endif // VR_BUILDER_SINGLE_LANE_PER_QUAD

#endif // VARIABLE_RATE_BUILDER_HLSLI
