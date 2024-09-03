#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRComputeStdDevDS, 1)]]

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
	half3 normal;
	float linearDepth;
	float luminance;
};


#define KERNEL_RADIUS 3

#define SHARED_MEMORY_X (8 + 2 * KERNEL_RADIUS)
#define SHARED_MEMORY_Y (4 + 2 * KERNEL_RADIUS)

groupshared SampleData sharedSampleData[SHARED_MEMORY_X][SHARED_MEMORY_Y];


float ComputeWeight(in SampleData center, in SampleData sample, int2 offset)
{
	const float kernel[4] = { 0.5f, 0.35f, 0.2f, 0.1f };

	const float phiNormal = 24.f;
	const float phiDepth = 0.1f;
	const float phiIllum = 0.1f;

	const float offsetWeight = kernel[abs(offset.x)] * kernel[abs(offset.y)];
	
	const float weightNormal = pow(saturate(dot(center.normal, sample.normal)), phiNormal);
	const float weightZ = (phiDepth == 0) ? 0.0f : abs(center.linearDepth - sample.linearDepth) / (phiDepth * offsetWeight);
	const float weightLuminance = abs(center.luminance - sample.luminance) / phiIllum;

	const float finalWeight = exp(0.0 - max(weightLuminance, 0.0) - max(weightZ, 0.0)) * weightNormal;

	return finalWeight;
}


[numthreads(8, 4, 1)]
void SRComputeStdDevCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_stdDevTexture.GetDimensions(outputRes.x, outputRes.y);

	const uint accumulatedSamplesNum = u_accumulatedSamplesNumTexture.Load(uint3(pixel, 0u)).x;

	const uint temporalVarianceRequiredSamplesNum = 6u;
	const uint useSpatialVarianceBallot = WaveActiveBallot(accumulatedSamplesNum < temporalVarianceRequiredSamplesNum).x;

	if(useSpatialVarianceBallot > 0)
	{
		const int2 tileSize = int2(8, 4);
		const int2 groupPixelOffset = int2(input.groupID.xy) * tileSize;

		const int sharedMemorySize = SHARED_MEMORY_X * SHARED_MEMORY_Y;

		for(uint i = input.localID.x + input.localID.y * 8; i < sharedMemorySize; i += 32)
		{
			const int x = i % SHARED_MEMORY_X;
			const int y = i / SHARED_MEMORY_X;

			const uint3 samplePixel = uint3(max(groupPixelOffset + int2(x, y) - KERNEL_RADIUS, int2(0, 0)), 0);

			sharedSampleData[x][y].normal      = half3(OctahedronDecodeNormal(u_normalsTexture.Load(samplePixel)));
			sharedSampleData[x][y].linearDepth = ComputeLinearDepth(u_depthTexture.Load(samplePixel).x, u_sceneView);
			sharedSampleData[x][y].luminance   = Luminance(u_luminanceTexture.Load(samplePixel).xyz);
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if(all(pixel < outputRes))
	{
		float standardDeviation = 0.f;
		if (accumulatedSamplesNum >= temporalVarianceRequiredSamplesNum)
		{
			const float2 moments = u_momentsTexture.Load(uint3(pixel, 0u)).xy;
			const float variance = abs(moments.y - moments.x * moments.x);
			standardDeviation = sqrt(variance);
		}
		else
		{
			const uint2 center = (input.localID.xy + KERNEL_RADIUS);

			const SampleData centerSample = sharedSampleData[center.x][center.y];

			float luminanceSum = 0.f;
			float luminanceSquaredSum = 0.f;
			float weightSum = 0.000001f;

			for (int y = -KERNEL_RADIUS; y <= KERNEL_RADIUS; ++y)
			{
				for (int x = -KERNEL_RADIUS; x <= KERNEL_RADIUS; ++x)
				{
					const uint2 sampleID = center + uint2(x, y);
					const SampleData sample = sharedSampleData[sampleID.x][sampleID.y];

					const float weight = ComputeWeight(centerSample, sample, int2(x, y));
					
					luminanceSum += sample.luminance * weight;
					luminanceSquaredSum += sample.luminance * sample.luminance * weight;
					weightSum += weight;
				}
			}

			luminanceSum        /= weightSum;
			luminanceSquaredSum /= weightSum;

			const float variance = abs(luminanceSquaredSum - luminanceSum * luminanceSum);
			standardDeviation = sqrt(variance);
			
			standardDeviation = max(luminanceSum * 0.6f, standardDeviation);
			standardDeviation *= (temporalVarianceRequiredSamplesNum - accumulatedSamplesNum);
		}

		u_stdDevTexture[pixel] = standardDeviation;
	}
}
