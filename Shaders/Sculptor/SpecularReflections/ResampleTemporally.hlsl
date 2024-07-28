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


#define HISTORY_SAMPLE_COUNT 5


static const int2 g_historyOffsets[HISTORY_SAMPLE_COUNT] =
{
	int2(0, 0),
	int2(-1, 0),
	int2(1, 0),
	int2(0, 1),
	int2(0, -1)
};


struct SRTemporalResampler
{
	MinimalSurfaceInfo centerPixelSurface;

	SRReservoir currentReservoir;

	MinimalGBuffer historyGBuffer;

	bool               foundHistorySample;
	SRReservoir        selectedHistoryReservoir;
	MinimalSurfaceInfo selectedHistorySurface;

	float selectedTargetPdf;

	static SRTemporalResampler Create(in MinimalSurfaceInfo inSurface, in SRReservoir initialReservoir)
	{
		SRTemporalResampler resampler;

		resampler.centerPixelSurface = inSurface;
		resampler.currentReservoir = SRReservoir::CreateEmpty();

		resampler.foundHistorySample = false;

		resampler.selectedTargetPdf = 0.f;

		resampler.historyGBuffer.depthTexture         = u_historyDepthTexture;
		resampler.historyGBuffer.normalsTexture       = u_historyNormalsTexture;
		resampler.historyGBuffer.specularColorTexture = u_historySpecularColorTexture;
		resampler.historyGBuffer.roughnessTexture     = u_historyRoughnessTexture;

		const float targetPdf = EvaluateTargetPdf(resampler.centerPixelSurface, initialReservoir.hitLocation, initialReservoir.luminance);
		resampler.currentReservoir.Update(initialReservoir, 0.f, targetPdf);
		resampler.selectedTargetPdf = targetPdf;

		return resampler;
	}

	bool FoundHistorySample()
	{
		return foundHistorySample;
	}

	bool TrySelectHistorySample(in uint2 historySamplePixel, inout RngState rng)
	{
		if (FoundHistorySample())
		{
			return true;
		}

		selectedHistorySurface = GetMinimalSurfaceInfo(historyGBuffer, historySamplePixel, u_prevFrameSceneView);

		if(!AreSurfacesSimilar(centerPixelSurface, selectedHistorySurface)
			|| !AreMaterialsSimilar(centerPixelSurface, selectedHistorySurface))
		{
			return false;
		}

		const uint historyReservoirIdx = GetScreenReservoirIdx(historySamplePixel, u_resamplingConstants.reservoirsResolution);
		selectedHistoryReservoir = UnpackReservoir(u_historyReservoirsBuffer[historyReservoirIdx]);

		if(!selectedHistoryReservoir.IsValid() || !currentReservoir.CanCombine(selectedHistoryReservoir))
		{
			return false;
		}

		const uint maxAge = 32 * lerp(rng.Next(), 0.6f, 1.f);

		if(selectedHistoryReservoir.age > maxAge)
		{
			return false;
		}

		if(!IsReservoirValidForSurface(selectedHistoryReservoir, centerPixelSurface))
		{
			return false;
		}

		foundHistorySample = true;
		return true;
	}

	void ResampleSelectedHistorySample(inout RngState rng)
	{
		if(foundHistorySample)
		{
			const uint maxHistoryLength = 10;
			selectedHistoryReservoir.M = min(selectedHistoryReservoir.M, maxHistoryLength);

			const float jacobian = EvaluateJacobian(centerPixelSurface.location, selectedHistorySurface.location, selectedHistoryReservoir);

			if(jacobian > 0.f)
			{
				selectedHistoryReservoir.weightSum *= jacobian;

				selectedHistoryReservoir.luminance = ExposedHistoryLuminanceToCurrentExposedLuminance(selectedHistoryReservoir.luminance);

				const float targetPdf = EvaluateTargetPdf(centerPixelSurface, selectedHistoryReservoir.hitLocation, selectedHistoryReservoir.luminance);

				if(currentReservoir.Update(selectedHistoryReservoir, rng.Next(), targetPdf))
				{
					currentReservoir.RemoveFlag(SR_RESERVOIR_FLAGS_RECENT);
					selectedTargetPdf = targetPdf;
				}
			}
		}
	}

	SRReservoir FinishResampling()
	{
		currentReservoir.age++;
		currentReservoir.Normalize(selectedTargetPdf);

		return currentReservoir;
	}
};


[numthreads(8, 8, 1)]
void ResampleTemporallyCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		MinimalGBuffer currentGBuffer;
		currentGBuffer.depthTexture         = u_depthTexture;
		currentGBuffer.normalsTexture       = u_normalsTexture;
		currentGBuffer.specularColorTexture = u_specularColorTexture;
		currentGBuffer.roughnessTexture     = u_roughnessTexture;

		float selectedTargetPdf = 0.f;

		const MinimalSurfaceInfo centerPixelSurface = GetMinimalSurfaceInfo(currentGBuffer, pixel, u_sceneView);

		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.reservoirsResolution);

		RngState rng = RngState::Create(pixel, u_resamplingConstants.frameIdx * 3);

		float reflectedRayLength = -1.f;
		SRTemporalResampler resampler;
		{
			const SRPackedReservoir packedReservoir = u_inReservoirsBuffer[reservoirIdx];

			if(centerPixelSurface.roughness <= 0.01f)
			{
				u_outReservoirsBuffer[reservoirIdx] = packedReservoir;
				return;
			}

			const SRReservoir reservoir = UnpackReservoir(packedReservoir);

			reflectedRayLength = distance(centerPixelSurface.location, reservoir.hitLocation);

			resampler = SRTemporalResampler::Create(centerPixelSurface, reservoir);
		}


		const float2 uv = (float2(pixel) + 0.5f) * u_resamplingConstants.pixelSize;

		const float2x2 samplesRotation = NoiseRotation(rng.Next());

		const float3 virtualReflectedLocation = centerPixelSurface.location - centerPixelSurface.v * reflectedRayLength;
		const float3 prevFrameNDCVirtual = WorldSpaceToNDC(virtualReflectedLocation, u_prevFrameSceneView);
		if(prevFrameNDCVirtual.z > 0.f && all(prevFrameNDCVirtual.xy <= 1.f) && all(prevFrameNDCVirtual.xy >= -1.f))
		{
			const float2 reprojectedUV = prevFrameNDCVirtual.xy * 0.5f + 0.5f;

			for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
			{
				const float2 sampleUV = reprojectedUV + mul(samplesRotation, g_historyOffsets[sampleIdx]) * u_resamplingConstants.pixelSize;
				const int2 samplePixel = round(sampleUV * u_resamplingConstants.resolution);
				
				if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
				{
					if(resampler.TrySelectHistorySample(samplePixel, INOUT rng))
					{
						break;
					}
				}
			}
		}

		if(!resampler.FoundHistorySample())
		{
			const float2 motion    = u_motionTexture.Load(uint3(pixel, 0)).xy;
			const float2 historyUV = uv - motion;

			if(all(historyUV >= 0.f) && all(historyUV <= 1.f))
			{
				for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
				{
					const float2 sampleUV = historyUV + mul(samplesRotation, g_historyOffsets[sampleIdx]) * u_resamplingConstants.pixelSize;
					const int2 samplePixel = round(sampleUV * u_resamplingConstants.resolution);
					
					if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
					{
						if(resampler.TrySelectHistorySample(samplePixel, INOUT rng))
						{
							break;
						}
					}
				}
			}
		}

		resampler.ResampleSelectedHistorySample(INOUT rng);

		SRReservoir newReservoir = resampler.FinishResampling();

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

