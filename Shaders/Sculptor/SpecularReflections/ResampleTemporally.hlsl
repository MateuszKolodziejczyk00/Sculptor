#include "SculptorShader.hlsli"

[[descriptor_set(SRTemporalResamplingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SRResamplingCommon.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/Wave.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
    uint3 groupID  : SV_GroupID;
    uint3 localID  : SV_GroupThreadID;
};


groupshared float s_averageReservoirWeight[2];


#define HISTORY_SAMPLE_COUNT 2


static const int2 g_historyOffsets[HISTORY_SAMPLE_COUNT] =
{
	int2(0, 0),
	int2(1, 0),
};


struct SRTemporalResampler
{
	MinimalSurfaceInfo centerPixelSurface;

	SRReservoir currentReservoir;

	MinimalGBuffer historyGBuffer;

	float selectedTargetPdf;

	bool m_wasSampleTraced;

	static SRTemporalResampler Create(in MinimalSurfaceInfo inSurface, in SRReservoir initialReservoir, bool wasSampleTraced)
	{
		SRTemporalResampler resampler;

		resampler.centerPixelSurface = inSurface;
		resampler.currentReservoir = SRReservoir::CreateEmpty();

		resampler.selectedTargetPdf = 0.f;

		resampler.historyGBuffer.depthTexture         = u_historyDepthTexture;
		resampler.historyGBuffer.normalsTexture       = u_historyNormalsTexture;
		resampler.historyGBuffer.specularColorTexture = u_historySpecularColorTexture;
		resampler.historyGBuffer.roughnessTexture     = u_historyRoughnessTexture;

		const float targetPdf = EvaluateTargetPdf(resampler.centerPixelSurface, initialReservoir.hitLocation, initialReservoir.luminance);
		resampler.currentReservoir.Update(initialReservoir, 0.f, targetPdf);
		resampler.selectedTargetPdf = targetPdf;

		resampler.m_wasSampleTraced = wasSampleTraced;

		return resampler;
	}

	bool TrySelectHistorySample(in uint2 historySamplePixel, inout RngState rng)
	{
		const MinimalSurfaceInfo selectedHistorySurface = GetMinimalSurfaceInfo(historyGBuffer, historySamplePixel, u_prevFrameSceneView);

		if(!AreSurfacesSimilar(centerPixelSurface, selectedHistorySurface)
			|| !AreMaterialsSimilar(centerPixelSurface, selectedHistorySurface))
		{
			return false;
		}

		const uint historyReservoirIdx = GetScreenReservoirIdx(historySamplePixel, u_resamplingConstants.reservoirsResolution);
		SRReservoir historyReservoir = UnpackReservoir(u_historyReservoirsBuffer[historyReservoirIdx]);

		if(!historyReservoir.IsValid() || !currentReservoir.CanCombine(historyReservoir))
		{
			return false;
		}

		const uint maxAge = 32 * lerp(rng.Next(), 0.6f, 1.f);

		if(m_wasSampleTraced && historyReservoir.age > maxAge)
		{
			return false;
		}

		if(!IsReservoirValidForSurface(historyReservoir, centerPixelSurface))
		{
			return false;
		}

		const uint maxHistoryLength = 10;
		historyReservoir.M = min(historyReservoir.M, maxHistoryLength);

		const float jacobian = EvaluateJacobian(centerPixelSurface.location, selectedHistorySurface.location, historyReservoir);

		if(jacobian <= 0.f)
		{
			return false;
		}

		historyReservoir.weightSum *= jacobian;

		historyReservoir.luminance = ExposedHistoryLuminanceToCurrentExposedLuminance(historyReservoir.luminance);

		const float targetPdf = EvaluateTargetPdf(centerPixelSurface, historyReservoir.hitLocation, historyReservoir.luminance);

		if(currentReservoir.Update(historyReservoir, rng.Next(), targetPdf))
		{
			currentReservoir.RemoveFlag(SR_RESERVOIR_FLAGS_RECENT);
			selectedTargetPdf = targetPdf;

			return true;
		}

		return false;
	}

	SRReservoir FinishResampling()
	{
		currentReservoir.age++;
		currentReservoir.Normalize(selectedTargetPdf);

		return currentReservoir;
	}
};

void AppendAdditionalTraceCommand(in uint2 traceCoords)
{
	const uint commandsNum = WaveActiveCountBits(true);

	uint appendOffset = 0u;
	if (WaveIsFirstLane())
	{
		InterlockedAdd(u_commandsNum[0], commandsNum, OUT appendOffset);

		uint tracesNum = 0;
		InterlockedAdd(u_tracesNum[0], commandsNum, OUT tracesNum);
		InterlockedMax(u_tracesDispatchGroupsNum[0], (tracesNum + commandsNum + 31) / 32);
	}

	const uint2 ballot = WaveActiveBallot(true).xy;
	appendOffset = WaveReadLaneFirst(appendOffset) + GetCompactedIndex(ballot, WaveGetLaneIndex());

	RayTraceCommand traceCommand;
	traceCommand.blockCoords      = traceCoords;
	traceCommand.localOffset      = uint2(0, 0);
	traceCommand.variableRateMask = SPT_VARIABLE_RATE_1X1;
	const EncodedRayTraceCommand encodedTraceCommand = EncodeTraceCommand(traceCommand);

	u_rayTracesCommands[appendOffset] = encodedTraceCommand;

	u_rwVariableRateBlocksTexture[traceCoords] = PackVRBlockInfo(uint2(0, 0), SPT_VARIABLE_RATE_1X1);
}


[numthreads(8, 8, 1)]
void ResampleTemporallyCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

	if(all(pixel == 0))
	{
		u_commandsNum[1] = 1;
		u_commandsNum[2] = 1;
	}
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		const float depth = u_depthTexture.Load(uint3(pixel, 0));
		if(depth <= 0.f)
		{
			return;
		}

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

			const SRReservoir reservoir = UnpackReservoir(packedReservoir);

			reflectedRayLength = distance(centerPixelSurface.location, reservoir.hitLocation);

			const uint2 traceCoords = GetVariableTraceCoords(u_rwVariableRateBlocksTexture, pixel);
			const bool wasSampleTraced = all(traceCoords == pixel);

			resampler = SRTemporalResampler::Create(centerPixelSurface, reservoir, wasSampleTraced);
		}

		if (centerPixelSurface.roughness > 0.01f)
		{
			const float2 uv = (float2(pixel) + 0.5f) * u_resamplingConstants.pixelSize;

			const float2 motion = u_motionTexture.Load(uint3(pixel, 0)).xy;
			const float2 reprojectedSurfaceUV  = uv - motion;

			const float3 virtualReflectedLocation = centerPixelSurface.location - centerPixelSurface.v * reflectedRayLength;
			const float3 prevFrameNDCVirtual = WorldSpaceToNDC(virtualReflectedLocation, u_prevFrameSceneView);

			const float2 reprojectedUVBasedOnVirtualPoint = prevFrameNDCVirtual.xy * 0.5f + 0.5f;

			const float surfaceReprojectionWeight = prevFrameNDCVirtual.z > 0.f ? saturate((centerPixelSurface.roughness - 0.1f) * 2.8f) : 1.f;

			const float2 reprojectedUV = lerp(reprojectedUVBasedOnVirtualPoint, reprojectedSurfaceUV, surfaceReprojectionWeight);

			if(all(reprojectedUV >= 0.f) && all(reprojectedUV <= 1.f))
			{
				const float2x2 samplesRotation = NoiseRotation(rng.Next());

				for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
				{
					const float2 sampleUV = reprojectedUV + mul(samplesRotation, g_historyOffsets[sampleIdx]) * u_resamplingConstants.pixelSize;
					const int2 samplePixel = round(sampleUV * u_resamplingConstants.resolution);
					
					if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
					{
						resampler.TrySelectHistorySample(samplePixel, INOUT rng);
					}
				}
			}
		}

		SRReservoir newReservoir = resampler.FinishResampling();
		const uint invalidReservoirs = WaveActiveCountBits(!newReservoir.IsValid());
		if((invalidReservoirs > 16u || centerPixelSurface.roughness <= 0.01f) && !newReservoir.IsValid())
		{
			AppendAdditionalTraceCommand(pixel);
		}

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

		if(centerPixelSurface.roughness > 0.01f && finalReservoirWeight > maxWeight)
		{
			newReservoir.weightSum = 0.f;
		}

		u_outReservoirsBuffer[reservoirIdx] = PackReservoir(newReservoir);
	}
}

