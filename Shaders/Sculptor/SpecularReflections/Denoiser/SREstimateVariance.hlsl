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
	float depth;
	float variance;
	half3 normal;
	half  roughness;
};


#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16


#define KERNEL_RADIUS 3


#if HORIZONTAL_PASS
#define SHARED_MEMORY_X (GROUP_SIZE_X + 2 * KERNEL_RADIUS)
#define SHARED_MEMORY_Y (GROUP_SIZE_Y)
#else
#define SHARED_MEMORY_X (GROUP_SIZE_X)
#define SHARED_MEMORY_Y (GROUP_SIZE_Y + 2 * KERNEL_RADIUS)
#endif // HORIZONTAL_PASS

groupshared SampleData sharedSampleData[SHARED_MEMORY_X][SHARED_MEMORY_Y];


float ApplyBesselsCorrection(in float variance, in float samplesNum)
{
	const float correction = samplesNum / max(samplesNum - 1.f, 0.4f);
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
		sample.roughness = half(u_roughnessTexture.Load(sampleCoords).x);
		sample.depth     = u_depthTexture.Load(sampleCoords).x;
		sample.normal    = half3(OctahedronDecodeNormal(u_normalsTexture.Load(sampleCoords).xy));

		const float variance   = u_varianceTexture.Load(sampleCoords).x;
		const float samplesNum = u_accumulatedSamplesNumTexture.Load(sampleCoords).x;

		sample.variance = ApplyBesselsCorrection(variance, samplesNum);
		
		sharedSampleData[localOffset.x][localOffset.y] = sample;
	}
}


float ComputeWeight(in SampleData center, in SampleData sample, int offset)
{
	const float phiNormal = 12.f;
	const float phiDepth = 0.15f;
	
	const float weightNormal = pow(saturate(dot(center.normal, sample.normal)), phiNormal);

	const float depthDifference = abs(center.depth - sample.depth);
	const float weightZ = depthDifference <= phiDepth ? 1.f : 0.f;

	const float finalWeight = exp(-max(weightZ, 0.0)) * weightNormal * u_constants.weights[abs(offset)];

	return finalWeight;
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

		const SampleData centerSample = sharedSampleData[centerLDSCoords.x][centerLDSCoords.y];

		const float centerWeight = u_constants.weights[0u];

		float varianceEstimationSum = centerSample.variance * centerWeight;
		float weightSum = centerWeight;

		[unroll]
		for (int offset = 1; offset <= KERNEL_RADIUS; ++offset)
		{
			const int2 ldsCoords0 = centerLDSCoords + offset * filterDir;
			const SampleData sample0 = sharedSampleData[ldsCoords0.x][ldsCoords0.y];

			const int2 ldsCoords1 = centerLDSCoords - offset * filterDir;
			const SampleData sample1 = sharedSampleData[ldsCoords1.x][ldsCoords1.y];

			const float weight0 = ComputeWeight(centerSample, sample0, offset);
			const float weight1 = ComputeWeight(centerSample, sample1, -offset);

			varianceEstimationSum += sample0.variance * weight0;
			weightSum += weight0;

			varianceEstimationSum += sample1.variance * weight1;
			weightSum += weight1;
		}

		const float varianceEstimation = varianceEstimationSum / weightSum;

		u_rwVarianceEstimationTexture[coords] = varianceEstimation;
	}
}
