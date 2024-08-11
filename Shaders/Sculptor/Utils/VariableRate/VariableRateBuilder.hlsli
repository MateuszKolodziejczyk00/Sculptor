#ifndef VARIABLE_RATE_BUILDER_HLSLI
#define VARIABLE_RATE_BUILDER_HLSLI

[[descriptor_set(CreateVariableRateTextureDS, 0)]]

#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/MortonCode.hlsli"


#define VRT_SIGNAL_VISIBILITY 0
#define VRT_SIGNAL_LIGHTING   1

#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 4

#define GS_SAMPLES_X (GROUP_SIZE_X + 2)
#define GS_SAMPLES_Y (GROUP_SIZE_Y + 2)


groupshared float gs_inputSamples[GS_SAMPLES_X][GS_SAMPLES_Y];


void CacheInputSamples(in Texture2D<float3> texture, in int2 tileCoords, in int threadIdx)
{
	for(int i = threadIdx; i < (GS_SAMPLES_X * GS_SAMPLES_Y); i += WaveGetLaneCount())
	{
		const int2 sampleCoords = int2(i % GS_SAMPLES_X, i / GS_SAMPLES_X);
		const int2 globalCoords = clamp(tileCoords + sampleCoords - int2(1, 1), int2(0, 0), u_constants.inputResolution);

		const float3 inputSample = texture.Load(uint3(globalCoords, 0));

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


#define FAST_VARIABLE_RATE 1


void WriteCurrentFrameVariableRateData(in RWTexture2D<uint> vrTexture, in uint2 coords, in uint vrMask, in uint frameIdx)
{
	const uint logFramesPerSlot  = u_constants.logFramesNumPerSlot;
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
	static VariableRateProcessor Create(in uint2 globalCoords, in uint localThreadIdx)
	{
		VariableRateProcessor processor;
		processor.m_globalCoords   = globalCoords;
		processor.m_localThreadIdx = localThreadIdx;
		return processor;
	}

	template<typename T>
	T QuadMax(in T value)
	{
		T result = value;
		const uint quadBaseThreadIdx = m_localThreadIdx & ~3;
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 0));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		result = max(result, WaveReadLaneAt(value, quadBaseThreadIdx + 3));
		return result;
	}

	template<typename T>
	T QuadMin(in T value)
	{
		T result = value;
		const uint quadBaseThreadIdx = m_localThreadIdx & ~3;
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 0));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 1));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 2));
		result = min(result, WaveReadLaneAt(value, quadBaseThreadIdx + 3));
		return result;
	}

	uint ComputeEdgeBasedVariableRateMask(in float2 edgeFilter)
	{
		uint variableRate = SPT_VARIABLE_RATE_1X1;

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.x < u_constants.xThreshold4)
		{
			variableRate |= SPT_VARIABLE_RATE_4X;
		}
		else
#endif // SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.x < u_constants.xThreshold2)
		{
			variableRate |= SPT_VARIABLE_RATE_2X;
		}

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.y < u_constants.yThreshold4)
		{
			variableRate |= SPT_VARIABLE_RATE_4Y;
		}
		else
#endif // SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
		if(edgeFilter.y < u_constants.yThreshold2)
		{
			variableRate |= SPT_VARIABLE_RATE_2Y;
		}

		return variableRate;
	}

	uint2 GetCoords()
	{
		return m_globalCoords;
	}

	uint2 m_globalCoords;
	uint m_localThreadIdx;
};


struct VariableRateBuilder
{
	template<typename TCallback>
	static void BuildVariableRateTexture(in uint2 groupID, in int localThreadIdx)
	{
		const uint2 tileCoords = groupID.xy * uint2(GROUP_SIZE_X, GROUP_SIZE_Y);
		CacheInputSamples(u_inputTexture, tileCoords, localThreadIdx);

		GroupMemoryBarrierWithGroupSync();

		const uint2 localID = DecodeMorton2D(localThreadIdx);

		const uint2 globalID = groupID * uint2(GROUP_SIZE_X, GROUP_SIZE_Y) + localID;

		float2 edgeFilter = SobelFilter(localID);

		VariableRateProcessor processor = VariableRateProcessor::Create(globalID, localThreadIdx);
		edgeFilter = processor.QuadMax(edgeFilter);

		const uint variableRate = TCallback::ComputeVariableRateMask(processor, edgeFilter);

		const uint2 outputCoords = groupID.xy * uint2(4, 2) + localID.xy / 2;
		if ((localThreadIdx & 3) == 0)
		{
			WriteCurrentFrameVariableRateData(u_rwVariableRateTexture, outputCoords, variableRate, u_constants.frameIdx);
		}
	}
};


struct GenericVariableRateCallback
{
	static uint ComputeVariableRateMask(in VariableRateProcessor processor, in float2 edgeFilter)
	{
		return processor.ComputeEdgeBasedVariableRateMask(edgeFilter);
	}
};


#endif // VARIABLE_RATE_BUILDER_HLSLI