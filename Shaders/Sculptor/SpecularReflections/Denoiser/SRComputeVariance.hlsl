#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRComputeVarianceDS, 1)]]

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
	half3  normal;
	float linearDepth;
	half  specularLum;
	half  diffuseLum;
};


#define KERNEL_RADIUS 3

#define SHARED_MEMORY_X (8 + 2 * KERNEL_RADIUS)
#define SHARED_MEMORY_Y (4 + 2 * KERNEL_RADIUS)

groupshared SampleData sharedSampleData[SHARED_MEMORY_X][SHARED_MEMORY_Y];


float ComputeWeight(in SampleData center, in SampleData sample, int2 offset)
{
	const float phiNormal = 12.f;
	const float phiDepth = 0.15f;
	
	const float weightNormal = pow(saturate(dot(center.normal, sample.normal)), phiNormal);

	const float depthDifference = abs(center.linearDepth - sample.linearDepth);
	const float weightZ = depthDifference <= phiDepth ? 1.f : 0.f;

	const float finalWeight = exp(-max(weightZ, 0.0)) * weightNormal;

	return finalWeight;
}


[numthreads(8, 4, 1)]
void SRComputeVarianceCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	const uint accumulatedSamplesNum = u_accumulatedSamplesNumTexture.Load(uint3(pixel, 0u)).x;

	const uint temporalVarianceRequiredSamplesNum = 4u;
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
			sharedSampleData[x][y].specularLum = half(Luminance(u_specularTexture.Load(samplePixel).xyz));
			sharedSampleData[x][y].diffuseLum  = half(Luminance(u_diffuseTexture.Load(samplePixel).xyz));
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if(all(pixel < u_constants.resolution))
	{
		const float2 specularMoments = u_specularMomentsTexture.Load(uint3(pixel, 0u)).xy;
		float specularVariance = abs(specularMoments.y - specularMoments.x * specularMoments.x);

		const float2 diffuseMoments = u_diffuseMomentsTexture.Load(uint3(pixel, 0u)).xy;
		float diffuseVariance = abs(diffuseMoments.y - diffuseMoments.x * diffuseMoments.x);

		if (accumulatedSamplesNum < temporalVarianceRequiredSamplesNum)
		{
			const uint2 center = (input.localID.xy + KERNEL_RADIUS);

			const SampleData centerSample = sharedSampleData[center.x][center.y];

			float specularLumSum   = 0.f;
			float specularLumSqSum = 0.f;
			float diffuseLumSum    = 0.f;
			float diffuseLumSqSum  = 0.f;

			float weightSum = 0.000001f;

			for (int y = -KERNEL_RADIUS; y <= KERNEL_RADIUS; ++y)
			{
				for (int x = -KERNEL_RADIUS; x <= KERNEL_RADIUS; ++x)
				{
					const uint2 sampleID = center + uint2(x, y);
					const SampleData sample = sharedSampleData[sampleID.x][sampleID.y];

					const float weight = ComputeWeight(centerSample, sample, int2(x, y));

					weightSum += weight;

					specularLumSum   += sample.specularLum * weight;
					specularLumSqSum += Pow2(sample.specularLum) * weight;

					diffuseLumSum   += sample.diffuseLum * weight;
					diffuseLumSqSum += Pow2(sample.diffuseLum) * weight;
				}
			}

			const float rcpWeightSum = weightSum > 0.f ? rcp(weightSum) :0.f;

			const float2 specularMoments = float2(specularLumSum, specularLumSqSum) * rcpWeightSum;
			const float2 diffuseMoments  = float2(diffuseLumSum, diffuseLumSqSum) * rcpWeightSum;

			float specularSpatialVariance = abs(specularMoments.y - Pow2(specularMoments.x));
			float diffuseSpatialVariance  = abs(diffuseMoments.y - Pow2(diffuseMoments.x));

			const float spatialVarianceBoost = 7.f;

			specularSpatialVariance *= (temporalVarianceRequiredSamplesNum - accumulatedSamplesNum + 1u) * spatialVarianceBoost;
			diffuseSpatialVariance  *= (temporalVarianceRequiredSamplesNum - accumulatedSamplesNum + 1u) * spatialVarianceBoost;

			specularVariance = max(specularVariance, specularSpatialVariance);
			diffuseVariance  = max(diffuseVariance, diffuseSpatialVariance);
		}

		u_rwVarianceTexture[pixel] = float2(specularVariance, diffuseVariance);
	}
}
