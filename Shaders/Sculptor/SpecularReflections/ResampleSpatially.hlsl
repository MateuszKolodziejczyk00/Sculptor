#include "SculptorShader.hlsli"

[[descriptor_set(SRSpatialResamplingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SRResamplingCommon.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define SPATIAL_RESAMPLING_SAMPLES_NUM 9


static const float2 resamplingSamples[SPATIAL_RESAMPLING_SAMPLES_NUM] = {
	float2(-0.474, 0.054),
	float2(0.650, -0.644),
	float2(0.136, 0.580),
	float2(0.914, 0.802),
	float2(-0.753, -0.732),
	float2(-0.216, -0.448),
	float2(0.368, -0.864),
	float2(-0.522, 0.742),
	float2(0.484, 0.006)
};


#define RESAMPLE_RADIUS 5.f


MinimalSurfaceInfo GetMinimalSurfaceInfo(in uint2 pixel)
{
	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	const float2 uv = (pixel + 0.5f) * u_resamplingConstants.pixelSize;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 sampleLocation = NDCToWorldSpace(ndc, u_sceneView);
	const float3 sampleNormal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;
	const float3 toView = normalize(u_sceneView.viewLocation - sampleLocation);

	const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));
	const float3 f0 = u_specularColorTexture.Load(uint3(pixel, 0)).rgb;

	MinimalSurfaceInfo pixelSurface;
	pixelSurface.location = sampleLocation;
	pixelSurface.n = sampleNormal;
	pixelSurface.v = toView;
	pixelSurface.f0 = f0;
	pixelSurface.roughness = roughness;

	return pixelSurface;
}


[numthreads(8, 8, 1)]
void ResampleSpatiallyCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.resolution);
		const SRReservoir reservoir = UnpackReservoir(u_inReservoirsBuffer[reservoirIdx]);

		SRReservoir newReservoir = SRReservoir::CreateEmpty();

		if(reservoir.HasFlag(SR_RESERVOIR_FLAGS_DDGI_TRACE))
		{
			newReservoir = reservoir;

			u_outReservoirsBuffer[reservoirIdx] = PackReservoir(newReservoir);
			return;
		}

		float selectedTargetPdf = 0.f;

		const MinimalSurfaceInfo pixelSurface = GetMinimalSurfaceInfo(pixel);
		{
			const float targetPdf = EvaluateTargetPdf(pixelSurface, reservoir.hitLocation, reservoir.luminance);

			newReservoir.Update(reservoir, 1.f, targetPdf);

			selectedTargetPdf = targetPdf;
		}

		RngState rng = RngState::Create(PCGHashFloat2(pixel, u_resamplingConstants.frameIdx));

		const float2x2 randomRotation = NoiseRotation(rng.Next());

		for(uint sampleIdx = 0; sampleIdx < SPATIAL_RESAMPLING_SAMPLES_NUM; ++sampleIdx)
		{
			const int2 offset = mul(resamplingSamples[sampleIdx] * RESAMPLE_RADIUS, randomRotation);
			const int2 samplePixel = pixel + offset;

			if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
			{
				const MinimalSurfaceInfo reuseSurface = GetMinimalSurfaceInfo(samplePixel);

				if(!AreSurfacesSimilar(pixelSurface, reuseSurface)
					|| !AreMaterialsSimilar(pixelSurface, reuseSurface))
				{
					continue;
				}

				const uint reuseReservoirIdx = GetScreenReservoirIdx(samplePixel, u_resamplingConstants.resolution);
				const SRReservoir reuseReservoir = UnpackReservoir(u_inReservoirsBuffer[reuseReservoirIdx]);

				if(!newReservoir.CanCombine(reuseReservoir))
				{
					continue;
				}

				float jacobian = EvaluateJacobian(pixelSurface.location, reuseSurface.location, reuseReservoir);

				if(!ValidateJabobian(INOUT jacobian))
				{
					continue;
				}

				const float targetPdf = EvaluateTargetPdf(reuseSurface, reuseReservoir.hitLocation, reuseReservoir.luminance);
				if(isnan(targetPdf) || isinf(targetPdf))
				{
					continue;
				}

				if(newReservoir.Update(reuseReservoir, rng.Next(), targetPdf * jacobian))
				{
					selectedTargetPdf = targetPdf;
				}
			}
		}

		newReservoir.Normalize(selectedTargetPdf);

		u_outReservoirsBuffer[reservoirIdx] = PackReservoir(newReservoir);
	}
}
