#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SREstimateVarianceDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


struct SampleData
{
	float2 variance;
	half3 normal;
};


#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16


#define KERNEL_RADIUS 3


#if HORIZONTAL_PASS
#define SHARED_MEMORY_X (GROUP_SIZE_X + 2 * KERNEL_RADIUS)
#define SHARED_MEMORY_Y (GROUP_SIZE_Y)
groupshared SampleData sharedSampleData[SHARED_MEMORY_Y][SHARED_MEMORY_X];
#else
#define SHARED_MEMORY_X (GROUP_SIZE_X)
#define SHARED_MEMORY_Y (GROUP_SIZE_Y + 2 * KERNEL_RADIUS)
groupshared SampleData sharedSampleData[SHARED_MEMORY_X][SHARED_MEMORY_Y];
#endif // HORIZONTAL_PASS


float2 ApplyBesselsCorrection(in float2 variance, in float2	 samplesNum)
{
	const float2 correction = samplesNum / max(samplesNum - 1.f, 0.4f);
	return variance * correction;
}


void CacheSamplesToLDS(in uint2 groupID, in uint2 threadID)
{
#if HORIZONTAL_PASS
	const int2 groupOffset = groupID * int2(GROUP_SIZE_X, GROUP_SIZE_Y) - int2(KERNEL_RADIUS, 0);
#else
	const int2 groupOffset = groupID * int2(GROUP_SIZE_X, GROUP_SIZE_Y) - int2(0, KERNEL_RADIUS);
#endif // HORIZONTAL_PASS

	const uint threadIdx = threadID.y * GROUP_SIZE_X + threadID.x;

	for(uint sampleIdx = threadIdx; sampleIdx < SHARED_MEMORY_X * SHARED_MEMORY_Y; sampleIdx += GROUP_SIZE_X * GROUP_SIZE_Y)
	{
		const int2 localOffset   = int2(sampleIdx % SHARED_MEMORY_X, sampleIdx / SHARED_MEMORY_X);
		const uint3 sampleCoords = uint3(clamp(int2(groupOffset + localOffset), 0, int2(u_constants.resolution - 1)), 0);

		SampleData sample;
		sample.normal = half3(OctahedronDecodeNormal(u_normalsTexture.Load(sampleCoords).xy));

		float2 variance = u_inVarianceTexture.Load(sampleCoords);

#if HORIZONTAL_PAS
		const float specularSamplesNum = u_specularHistoryLengthTexture.Load(sampleCoords).x;
		const float diffuseSamplesNum  = u_diffuseHistoryLengthTexture.Load(sampleCoords).x;
		variance = ApplyBesselsCorrection(variance, float2(specularSamplesNum, diffuseSamplesNum));
#endif // HORIZONTAL_PASS

		sample.variance = variance;
		
#if HORIZONTAL_PASS
		sharedSampleData[localOffset.y][localOffset.x] = sample;
#else
		sharedSampleData[localOffset.x][localOffset.y] = sample;
#endif // HORIZONTAL_PASS
	}
}


float ComputeWeight(in SampleData center, in SampleData sample, int offset)
{
	const float phiNormal = 12.f;
	return pow(saturate(dot(center.normal, sample.normal)), phiNormal);
}


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void SREstimateVarianceCS(CS_INPUT input)
{
	CacheSamplesToLDS(input.groupID.xy, input.localID.xy);

	const uint2 coords = input.globalID.xy;

	uint2 outputRes;
	u_rwVarianceEstimationTexture.GetDimensions(outputRes.x, outputRes.y);

	GroupMemoryBarrierWithGroupSync();

	if(all(coords < u_constants.resolution))
	{
#if HORIZONTAL_PASS
		const int2 centerLDSCoords = int2(input.localID.x + KERNEL_RADIUS, input.localID.y);
		const int2 filterDir = int2(1, 0);
#else
		const int2 centerLDSCoords = int2(input.localID.x, input.localID.y + KERNEL_RADIUS);
		const int2 filterDir = int2(0, 1);
#endif // HORIZONTAL_PASS

#if HORIZONTAL_PASS
		const SampleData centerSample = sharedSampleData[centerLDSCoords.y][centerLDSCoords.x];
#else
		const SampleData centerSample = sharedSampleData[centerLDSCoords.x][centerLDSCoords.y];
#endif // HORIZONTAL_PASS

		float2 maxVariance = centerSample.variance;

		[unroll]
		for (int offset = 1; offset <= KERNEL_RADIUS; ++offset)
		{
			const int2 ldsCoords0 = centerLDSCoords + offset * filterDir;
			const int2 ldsCoords1 = centerLDSCoords - offset * filterDir;

#if HORIZONTAL_PASS
			const SampleData sample0 = sharedSampleData[ldsCoords0.y][ldsCoords0.x];
			const SampleData sample1 = sharedSampleData[ldsCoords1.y][ldsCoords1.x];
#else
			const SampleData sample0 = sharedSampleData[ldsCoords0.x][ldsCoords0.y];
			const SampleData sample1 = sharedSampleData[ldsCoords1.x][ldsCoords1.y];
#endif // HORIZONTAL_PASS

			const float weight0 = ComputeWeight(centerSample, sample0, offset);
			const float weight1 = ComputeWeight(centerSample, sample1, -offset);

			const float weightThreshold = 0.8f;

			if(weight0 >= weightThreshold)
			{
				maxVariance = max(maxVariance, sample0.variance);
			}
			if(weight1 >= weightThreshold)
			{
				maxVariance = max(maxVariance, sample1.variance);
			}
		}

		u_rwVarianceEstimationTexture[coords] = maxVariance;
	}
}
