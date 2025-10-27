#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRComputeVarianceDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/Denoiser/RTDenoising.hlsli"


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
};


#define KERNEL_RADIUS 3

#define SHARED_MEMORY_X (8 + 2 * KERNEL_RADIUS)
#define SHARED_MEMORY_Y (4 + 2 * KERNEL_RADIUS)

groupshared SampleDataGeometryData gs_samplesGeoData[SHARED_MEMORY_X][SHARED_MEMORY_Y];
groupshared RTSphericalBasisRaw gs_specularY_SH2[SHARED_MEMORY_X][SHARED_MEMORY_Y];
groupshared RTSphericalBasisRaw gs_diffuseY_SH2[SHARED_MEMORY_X][SHARED_MEMORY_Y];


float ComputeWeight(in SampleDataGeometryData center, in SampleDataGeometryData sample, int2 offset)
{
	const float phiNormal = 12.f;
	const float phiDepth = 0.15f;
	
	const float weightNormal = pow(saturate(dot(center.normal, sample.normal)), phiNormal);

	const float depthDifference = abs(center.linearDepth - sample.linearDepth);
	const float weightZ = depthDifference <= phiDepth ? 1.f : 0.f;

	const float finalWeight = exp(-max(weightZ, 0.f)) * weightNormal;

	SPT_CHECK_MSG(!isinf(finalWeight) && !isnan(finalWeight), L"Invalid weight");

	return finalWeight;
}


[numthreads(8, 4, 1)]
void SRComputeVarianceCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	const uint specularHistoryLength = u_specularHistoryLengthTexture.Load(uint3(pixel, 0u)).x;
	const uint diffuseHistoryLength  = u_diffuseHistoryLengthTexture.Load(uint3(pixel, 0u)).x;

	const uint temporalVarianceRequiredHistoryLength = 4u;

	const bool useSpecularSpatialVariance = specularHistoryLength < temporalVarianceRequiredHistoryLength;
	const uint useSpecularSpatialVarianceBallot = WaveActiveBallot(useSpecularSpatialVariance).x;
	const bool useDiffuseSpatialVariance = diffuseHistoryLength < temporalVarianceRequiredHistoryLength;
	const uint useDiffuseSpatialVarianceBallot = WaveActiveBallot(useDiffuseSpatialVariance).x;
	const uint useSpatialVarianceBallot = useSpecularSpatialVarianceBallot | useDiffuseSpatialVarianceBallot;

	if (useSpatialVarianceBallot > 0u)
	{
		const int2 tileSize = int2(8, 4);
		const int2 groupPixelOffset = int2(input.groupID.xy) * tileSize;

		const int sharedMemorySize = SHARED_MEMORY_X * SHARED_MEMORY_Y;

		for(uint i = input.localID.x + input.localID.y * 8; i < sharedMemorySize; i += 32)
		{
			const int x = i % SHARED_MEMORY_X;
			const int y = i / SHARED_MEMORY_X;

			const uint3 samplePixel = uint3(max(groupPixelOffset + int2(x, y) - KERNEL_RADIUS, int2(0, 0)), 0u);

			gs_samplesGeoData[x][y].normal      = half3(OctahedronDecodeNormal(u_normalsTexture.Load(samplePixel)));
			gs_samplesGeoData[x][y].linearDepth = ComputeLinearDepth(u_depthTexture.Load(samplePixel).x, u_sceneView);

			if(useSpecularSpatialVarianceBallot > 0u)
			{
				gs_specularY_SH2[x][y] = u_specularY_SH2.Load(samplePixel);
			}

			if(useDiffuseSpatialVarianceBallot > 0u)
			{
				gs_diffuseY_SH2[x][y] = u_diffuseY_SH2.Load(samplePixel);
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

		if (useSpecularSpatialVariance || useDiffuseSpatialVariance)
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
					const uint2 sampleID = center + int2(x, y);
					const SampleDataGeometryData sample = gs_samplesGeoData[sampleID.x][sampleID.y];

					const float weight = ComputeWeight(centerSample, sample, int2(x, y));

					weightSum += weight;

					if(useSpecularSpatialVariance)
					{
						const RTSphericalBasis specularY = RawToRTSphericalBasis(gs_specularY_SH2[sampleID.x][sampleID.y]);
						const float specularLum = specularY.Evaluate(centerSample.normal);

						specularLumSum   += specularLum * weight;
						specularLumSqSum += Pow2(specularLum) * weight;
					}

					if(useDiffuseSpatialVariance)
					{
						const RTSphericalBasis diffuseY = RawToRTSphericalBasis(gs_diffuseY_SH2[sampleID.x][sampleID.y]);
						const float diffuseLum = diffuseY.Evaluate(centerSample.normal);

						diffuseLumSum   += diffuseLum * weight;
						diffuseLumSqSum += Pow2(diffuseLum) * weight;
					}
				}
			}
			const float rcpWeightSum = weightSum > 0.f ? rcp(weightSum) : 0.f;

			const float2 specularMoments = float2(specularLumSum, specularLumSqSum) * rcpWeightSum;
			const float2 diffuseMoments  = float2(diffuseLumSum, diffuseLumSqSum) * rcpWeightSum;

			float specularSpatialVariance = abs(specularMoments.y - Pow2(specularMoments.x));
			float diffuseSpatialVariance  = abs(diffuseMoments.y - Pow2(diffuseMoments.x));

			const float spatialVarianceBoost = 4.f;

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
