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


struct SampleDataGeometryData
{
	half3  normal;
	float linearDepth;
	half  specularLum;
	half  diffuseLum;
};


#define KERNEL_RADIUS 3

#define SHARED_MEMORY_X (8 + 2 * KERNEL_RADIUS)
#define SHARED_MEMORY_Y (4 + 2 * KERNEL_RADIUS)

groupshared SampleDataGeometryData gs_samplesGeoData[SHARED_MEMORY_X][SHARED_MEMORY_Y];
groupshared half gs_specularLum[SHARED_MEMORY_X][SHARED_MEMORY_Y];
groupshared half gs_diffuseLum[SHARED_MEMORY_X][SHARED_MEMORY_Y];


float ComputeWeight(in SampleDataGeometryData center, in SampleDataGeometryData sample, int2 offset)
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

	const uint specularHistoryLength = u_specularHistoryLengthTexture.Load(uint3(pixel, 0u)).x;
	const uint diffuseHistoryLength  = u_diffuseHistoryLengthTexture.Load(uint3(pixel, 0u)).x;

	const uint temporalVarianceRequiredHistoryLength = 4u;

	const bool useSpatialVariance = specularHistoryLength < temporalVarianceRequiredHistoryLength || diffuseHistoryLength < temporalVarianceRequiredHistoryLength;
	const uint useSpatialVarianceBallot = WaveActiveBallot(useSpatialVariance).x;

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

			gs_samplesGeoData[x][y].normal      = half3(OctahedronDecodeNormal(u_normalsTexture.Load(samplePixel)));
			gs_samplesGeoData[x][y].linearDepth = ComputeLinearDepth(u_depthTexture.Load(samplePixel).x, u_sceneView);

			if(specularHistoryLength < temporalVarianceRequiredHistoryLength)
			{
				gs_specularLum[x][y] = half(Luminance(u_specularTexture.Load(samplePixel).xyz));;
			}

			if(diffuseHistoryLength < temporalVarianceRequiredHistoryLength)
			{
				gs_diffuseLum[x][y] = half(Luminance(u_diffuseTexture.Load(samplePixel).xyz));;
			}
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if(all(pixel < u_constants.resolution))
	{
		const float2 specularMoments = u_specularMomentsTexture.Load(uint3(pixel, 0u)).xy;
		float specularVariance = abs(specularMoments.y - specularMoments.x * specularMoments.x);

		const float2 diffuseMoments = u_diffuseMomentsTexture.Load(uint3(pixel, 0u)).xy;
		float diffuseVariance = abs(diffuseMoments.y - diffuseMoments.x * diffuseMoments.x);

		if (useSpatialVariance)
		{
			const uint2 center = (input.localID.xy + KERNEL_RADIUS);

			const SampleDataGeometryData centerSample = gs_samplesGeoData[center.x][center.y];

			float specularLumSum   = 0.f;
			float specularLumSqSum = 0.f;
			float diffuseLumSum   = 0.f;
			float diffuseLumSqSum = 0.f;

			float weightSum = 0.000001f;

			for (int y = -KERNEL_RADIUS; y <= KERNEL_RADIUS; ++y)
			{
				for (int x = -KERNEL_RADIUS; x <= KERNEL_RADIUS; ++x)
				{
					const uint2 sampleID = center + uint2(x, y);
					const SampleDataGeometryData sample = gs_samplesGeoData[sampleID.x][sampleID.y];

					const float weight = ComputeWeight(centerSample, sample, int2(x, y));

					weightSum += weight;

					if(specularHistoryLength < temporalVarianceRequiredHistoryLength)
					{
						specularLumSum   += sample.specularLum * weight;
						specularLumSqSum += Pow2(sample.specularLum) * weight;
					}

					if(diffuseHistoryLength < temporalVarianceRequiredHistoryLength)
					{
						diffuseLumSum   += sample.diffuseLum * weight;
						diffuseLumSqSum += Pow2(sample.diffuseLum) * weight;
					}
				}
			}
			const float rcpWeightSum = weightSum > 0.f ? rcp(weightSum) :0.f;

			const float2 specularMoments = float2(specularLumSum, specularLumSqSum) * rcpWeightSum;
			const float2 diffuseMoments  = float2(diffuseLumSum, diffuseLumSqSum) * rcpWeightSum;

			float specularSpatialVariance = abs(specularMoments.y - Pow2(specularMoments.x));
			float diffuseSpatialVariance  = abs(diffuseMoments.y - Pow2(diffuseMoments.x));

			const float spatialVarianceBoost = 7.f;

			specularSpatialVariance *= (temporalVarianceRequiredHistoryLength - specularHistoryLength + 1u) * spatialVarianceBoost;
			diffuseSpatialVariance  *= (temporalVarianceRequiredHistoryLength - diffuseHistoryLength + 1u) * spatialVarianceBoost;


			if(specularHistoryLength < temporalVarianceRequiredHistoryLength)
			{
				specularVariance = max(specularVariance, specularSpatialVariance);
			}

			if(diffuseHistoryLength < temporalVarianceRequiredHistoryLength)
			{
				diffuseVariance  = max(diffuseVariance, diffuseSpatialVariance);
			}
		}

		u_rwVarianceTexture[pixel] = float2(specularVariance, diffuseVariance);
	}
}
