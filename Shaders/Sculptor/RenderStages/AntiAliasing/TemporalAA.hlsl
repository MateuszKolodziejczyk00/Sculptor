#include "SculptorShader.hlsli"

[[shader_params(TemporalAAConstants, PARAMS_TEMPORAL_A_A)]]

#include "Utils/Sampling.hlsli"
#include "Utils/ColorSpaces.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define GROUP_SIZE 8

#define SHARED_DATA_SIZE (GROUP_SIZE + 2)

groupshared float sharedDepths[SHARED_DATA_SIZE][SHARED_DATA_SIZE];
groupshared float3 sharedColors[SHARED_DATA_SIZE][SHARED_DATA_SIZE];


void CacheGroupLocalDepths(CS_INPUT input, float2 pixelSize, int2 outputRes)
{
	for (uint i = input.localID.x + input.localID.y * GROUP_SIZE; i < SHARED_DATA_SIZE * SHARED_DATA_SIZE; i += GROUP_SIZE * GROUP_SIZE)
	{
		const uint x = i / SHARED_DATA_SIZE;
		const uint y = i - (x * SHARED_DATA_SIZE);

		int2 pixel = (input.groupID.xy * 8) - int2(1, 1) + int2(x, y);
		pixel.x = clamp(pixel.x, 0, outputRes.x - 1);
		pixel.y = clamp(pixel.y, 0, outputRes.y - 1);

		const float depth = PARAMS_TEMPORAL_A_A->depth.SampleLevel(BindlessSamplers::NearestClampEdge(), pixel * pixelSize, 0);
		sharedDepths[x][y] = depth;

		const float3 color = PARAMS_TEMPORAL_A_A->outputColor[pixel];
		sharedColors[x][y] = color;
	}
}


int2 FindClosestPixelOffset3x3(uint2 localID)
{
	float closestDepth = 0.f; // we use inverse depth
	int2 closestDepthPixelOffset = 0;
	for (int i = -1; i <= 1; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			const float neighbourDepth = sharedDepths[localID.x + i + 1][localID.y + j + 1];
			if(neighbourDepth > closestDepth)
			{
				closestDepth = neighbourDepth;
				closestDepthPixelOffset = int2(i, j);
			}
		}
	}

	return closestDepthPixelOffset;
}


// Based on https://github.com/TheRealMJP/MSAAFilter/blob/master/MSAAFilter/Resolve.hlsl
float FilterCubic(float x, float B, float C)
{
	float y = 0.0f;
	float x2 = x * x;
	float x3 = x * x * x;
	if(x < 1)
		y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
	else if (x <= 2)
		y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

	return y / 6.0f;
}


float FilterMitchell(float value)
{
	const float cubicValue = value;
	const float ab = rcp(3.0f);
	return FilterCubic(cubicValue, ab, ab);
}


float SubsampleWeight(float distance)
{
	// Cubic filters works on [-2, 2] domain, thus scale the value by 2
	return FilterMitchell(distance * 2.f);
}


[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TemporalAACS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	uint2 outputRes = PARAMS_TEMPORAL_A_A->outputColor.GetResolution();

	const float2 pixelSize = rcp(float2(outputRes));

	CacheGroupLocalDepths(input, pixelSize, outputRes);

	GroupMemoryBarrierWithGroupSync();
	
	const float2 uv = pixel * pixelSize + pixelSize * 0.5f;

	if (pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const int2 closestPixelOffset = FindClosestPixelOffset3x3(input.localID.xy);

		const float2 closestNeighborUV = uv + pixelSize * closestPixelOffset;
		
		const float2 motion = PARAMS_TEMPORAL_A_A->motion.SampleLevel(BindlessSamplers::NearestClampEdge(), closestNeighborUV, 0);

		const float2 reporojectedUV = uv - motion;

		if (all(reporojectedUV >= 0.f) && all(reporojectedUV <= 1.f))
		{
			float currentSampleWeight = 0.f;
			float3 currentSampleSum = 0.f;
			
			float3 neighborhoodMin = 9999.f;
			float3 neighborhoodMax = -9999.f;

			// Moments
			float3 m1 = 0.f;
			float3 m2 = 0.f;

			for(int y = -1; y <= 1; ++y)
			{
				for(int x = -1; x <= 1; ++x)
				{
					const int2 offset = int2(x, y);
					const float2 neighborUV = uv + pixelSize * offset;

					float3 neighborColor = PARAMS_TEMPORAL_A_A->inputColor.SampleLevel(BindlessSamplers::LinearClampEdge(), neighborUV, 0);
					neighborColor /= (Luminance(neighborColor) + 1.f);

					if(PARAMS_TEMPORAL_A_A->useYCoCg)
					{
						neighborColor = RGBToYCoCg(neighborColor);
					}

					const float subsampleDistance = length(float2(x, y));
					
					const float subsampleWeight = SubsampleWeight(subsampleDistance);

					currentSampleSum += neighborColor * subsampleWeight;
					currentSampleWeight += subsampleWeight;

					neighborhoodMin = min(neighborhoodMin, neighborColor);
					neighborhoodMax = max(neighborhoodMax, neighborColor);

					m1 += neighborColor;
					m2 += neighborColor * neighborColor;
				}
			}
			
			const float3 currentSample = currentSampleSum / currentSampleWeight;

			// Shrink chroma min-max
			if(PARAMS_TEMPORAL_A_A->useYCoCg)
			{
				const float chromaExtent = 0.125f * (neighborhoodMax.x - neighborhoodMin.x);
				const float2 chromaCenter = currentSample.yz;
				neighborhoodMax.yz = chromaCenter + chromaExtent;
				neighborhoodMin.yz = chromaCenter - chromaExtent;
			}

			float3 historySample = SampleCatmullRom(PARAMS_TEMPORAL_A_A->historyColor, BindlessSamplers::LinearClampEdge(), reporojectedUV, float2(outputRes));
			historySample *= (PARAMS_TEMPORAL_A_A->exposure[PARAMS_TEMPORAL_A_A->exposureOffset] / PARAMS_TEMPORAL_A_A->exposure[PARAMS_TEMPORAL_A_A->historyExposureOffset]);
			historySample /= (Luminance(historySample) + 1.f);

			if(PARAMS_TEMPORAL_A_A->useYCoCg)
			{
				historySample = RGBToYCoCg(historySample);
			}

			const float invSamplesNum = rcp(9.f);
			const float gamma = 1.f;
			const float3 mu = m1 * invSamplesNum;
			const float3 sigma = sqrt(abs((m2 * invSamplesNum) - (mu * mu)));
			const float3 minc = mu - gamma * sigma;
			const float3 maxc = mu + gamma * sigma;

			const float3 clampedhistorySample = clamp(historySample, neighborhoodMin, neighborhoodMax);
			historySample = ClipAABB(minc, maxc, clampedhistorySample);

			float3 historyWeight = 0.9f;
			float3 currentWeight = 1.f - historyWeight;

			const float currentLuminance = PARAMS_TEMPORAL_A_A->useYCoCg ? currentSample.r : Luminance(currentSample);
			const float historyLuminance = PARAMS_TEMPORAL_A_A->useYCoCg ? historySample.r : Luminance(historySample);
			
			historyWeight /= max(historyLuminance, 0.01f);
			currentWeight /= max(currentLuminance, 0.01f);

			historyWeight = max(historyWeight / (historyWeight + currentWeight), 0.9f);
			currentWeight = 1.f - historyWeight;
			
			float3 outputColor = (historyWeight * historySample + currentWeight * currentSample);
			outputColor /= (1.f - Luminance(outputColor));

			if(PARAMS_TEMPORAL_A_A->useYCoCg)
			{
				outputColor = YCoCgToRGB(outputColor);
			}

			PARAMS_TEMPORAL_A_A->outputColor[pixel] = (outputColor);
		}
		else
		{
			const float3 inputColor = PARAMS_TEMPORAL_A_A->inputColor.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0);
			PARAMS_TEMPORAL_A_A->outputColor[pixel] = inputColor;
		}
	}
}
