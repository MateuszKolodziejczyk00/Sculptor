#include "SculptorShader.hlsli"

[[descriptor_set(SRTemporalResamplingDS, 0)]]
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
    uint3 groupID  : SV_GroupID;
    uint3 localID  : SV_GroupThreadID;
};


groupshared float s_averageReservoirWeight[2];


static const int2 g_historyOffsets[9] =
{
	int2(0, 0),
	int2(-1, 0),
	int2(1, 0),
	int2(0, 1),
	int2(0, -1),
	int2(-1, -1),
	int2(1, -1),
	int2(-1, 1),
	int2(1, 1)
};


[numthreads(8, 8, 1)]
void ResampleTemporallyCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.resolution);
		const SRReservoir reservoir = UnpackReservoir(u_inReservoirsBuffer[reservoirIdx]);

		MinimalGBuffer currentGBuffer;
		currentGBuffer.depthTexture         = u_depthTexture;
		currentGBuffer.normalsTexture       = u_normalsTexture;
		currentGBuffer.specularColorTexture = u_specularColorTexture;
		currentGBuffer.roughnessTexture     = u_roughnessTexture;

		float selectedTargetPdf = 0.f;

		const MinimalSurfaceInfo centerPixelSurface = GetMinimalSurfaceInfo(currentGBuffer, pixel, u_sceneView);
		
		SRReservoir newReservoir = SRReservoir::CreateEmpty();

		{
			const float targetPdf = EvaluateTargetPdf(centerPixelSurface, reservoir.hitLocation, reservoir.luminance);

			newReservoir.Update(reservoir, 0.f, targetPdf);

			selectedTargetPdf = targetPdf;
		}

		MinimalGBuffer historyGBuffer;
		historyGBuffer.depthTexture         = u_historyDepthTexture;
		historyGBuffer.normalsTexture       = u_historyNormalsTexture;
		historyGBuffer.specularColorTexture = u_historySpecularColorTexture;
		historyGBuffer.roughnessTexture     = u_historyRoughnessTexture;

		RngState rng = RngState::Create(pixel.yx, u_resamplingConstants.frameIdx * 3);

		const float2 uv        = (float2(pixel) + 0.5f) * u_resamplingConstants.pixelSize;
		const float2 motion    = u_motionTexture.Load(uint3(pixel, 0)).xy;
		const float2 historyUV = uv - motion;

		if(all(historyUV >= 0.f) && all(historyUV <= 1.f))
		{
			const int2 reprojectedPixel = round(historyUV * u_resamplingConstants.resolution);

			bool foundHistorySample = false;
			MinimalSurfaceInfo historySurface;
			SRReservoir historyReservoir = SRReservoir::CreateEmpty();

			for(uint sampleIdx = 0; sampleIdx < 9; ++sampleIdx)
			{
				const int2 sampleOffset = g_historyOffsets[sampleIdx];
				const int2 samplePixel = reprojectedPixel + sampleOffset;
				
				if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
				{
					historySurface = GetMinimalSurfaceInfo(historyGBuffer, samplePixel, u_prevFrameSceneView);

					if(!AreSurfacesSimilar(centerPixelSurface, historySurface)
						|| !AreMaterialsSimilar(centerPixelSurface, historySurface))
					{
						continue;
					}

					const uint historyReservoirIdx = GetScreenReservoirIdx(samplePixel, u_resamplingConstants.resolution);
					historyReservoir = UnpackReservoir(u_historyReservoirsBuffer[historyReservoirIdx]);

					if(!historyReservoir.IsValid() || !newReservoir.CanCombine(historyReservoir))
					{
						continue;
					}

					const uint maxAge = 32 * lerp(rng.Next(), 0.6f, 1.f);

					if(historyReservoir.age > maxAge)
					{
						continue;
					}

					if(!IsReservoirValidForSurface(historyReservoir, centerPixelSurface))
					{
						continue;
					}

					foundHistorySample = true;
					break;
				}
			}

			if(foundHistorySample)
			{
				const uint maxHistoryLength = 10;
				historyReservoir.M = min(historyReservoir.M, maxHistoryLength);

				const float jacobian = EvaluateJacobian(centerPixelSurface.location, historySurface.location, historyReservoir);

				if(jacobian > 0.f)
				{
					historyReservoir.weightSum *= jacobian;

					historyReservoir.luminance = ExposedHistoryLuminanceToCurrentExposedLuminance(historyReservoir.luminance);

					const float targetPdf = EvaluateTargetPdf(centerPixelSurface, historyReservoir.hitLocation, historyReservoir.luminance);

					if(newReservoir.Update(historyReservoir, rng.Next(), targetPdf))
					{
						selectedTargetPdf = targetPdf;
					}
				}
			}
		}

		newReservoir.age++;

		newReservoir.Normalize(selectedTargetPdf);

		// Firefly filter
		const float finalReservoirWeight = Luminance(newReservoir.luminance) * newReservoir.weightSum;
		const float weightSum = WaveActiveSum(finalReservoirWeight);
		const float waveCount = WaveActiveCountBits(true);

		const uint waveIdx = WaveActiveAnyTrue(all(input.localID == 0));
		if(WaveIsFirstLane())
		{
			s_averageReservoirWeight[waveIdx] = weightSum / waveCount;
		}

		GroupMemoryBarrierWithGroupSync();

		const float averageWeight = (s_averageReservoirWeight[0] + s_averageReservoirWeight[1]) * 0.5f;

		const float maxWeight = averageWeight * 15.f;

		if(finalReservoirWeight > maxWeight)
		{
			newReservoir.weightSum = 0.f;
		}

		u_outReservoirsBuffer[reservoirIdx] = PackReservoir(newReservoir);
	}
}
