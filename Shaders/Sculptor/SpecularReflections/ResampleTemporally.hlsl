#include "SculptorShader.hlsli"

[[descriptor_set(SRTemporalResamplingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#if ENABLE_SECOND_TRACING_PASS
[[descriptor_set(SRAdditionalPassesAllocatorDS, 2)]]
#endif // ENABLE_SECOND_TRACING_PASS


#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
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


#define HISTORY_SAMPLE_COUNT 2


struct SRTemporalResampler
{
	MinimalSurfaceInfo centerPixelSurface;

	SRReservoir currentReservoir;

	MinimalGBuffer historyGBuffer;

	float m_selectedP_hat;

	uint m_validSamplesMask;
	int2 m_sampleCoords[HISTORY_SAMPLE_COUNT];
	int m_selectedSampleIdx;

	uint m_currentSampleIdx;

	uint m_initialM;

	bool m_wasSampleTraced;

	bool m_foundValidHistorySurface;

	static SRTemporalResampler Create(in MinimalSurfaceInfo inSurface, in SRReservoir initialReservoir, bool wasSampleTraced)
	{
		SRTemporalResampler resampler;

		resampler.centerPixelSurface = inSurface;
		resampler.currentReservoir = SRReservoir::CreateEmpty();
		resampler.currentReservoir.spatialResamplingRangeID = SR_RESERVOIR_DEFAULT_SPATIAL_RANGE_ID;

		resampler.historyGBuffer.depthTexture             = u_historyDepthTexture;
		resampler.historyGBuffer.normalsTexture           = u_historyNormalsTexture;
		resampler.historyGBuffer.baseColorMetallicTexture = u_historyBaseColorTexture;
		resampler.historyGBuffer.roughnessTexture         = u_historyRoughnessTexture;

		const float p_hat = EvaluateTargetFunction(resampler.centerPixelSurface, initialReservoir.hitLocation, initialReservoir.luminance);

		resampler.currentReservoir.flags = initialReservoir.flags;

		if(resampler.currentReservoir.Update(initialReservoir, 0.f, p_hat))
		{
			resampler.m_selectedP_hat = p_hat;
		}

		resampler.m_wasSampleTraced = wasSampleTraced;
		resampler.m_foundValidHistorySurface = false;

		resampler.m_validSamplesMask  = 0u;
		resampler.m_selectedSampleIdx = -1;
		resampler.m_currentSampleIdx  = 0;
		resampler.m_initialM          = initialReservoir.M;

		return resampler;
	}

	SRReservoir LoadHistoryReservoir(in uint2 coords)
	{
		const uint historyReservoirIdx = GetScreenReservoirIdx(coords, u_resamplingConstants.reservoirsResolution);
		SRReservoir historyReservoir = UnpackReservoir(u_historyReservoirsBuffer[historyReservoirIdx]);

		const uint maxHistoryLength = 30u;
		historyReservoir.M = uint16_t(min(historyReservoir.M, maxHistoryLength));

		return historyReservoir;
	}

	uint ComputeReservoirMaxAge(in int2 historySamplePixel)
	{
		float maxAge;

		// Reduce max age for close hits to maintain close indirect shadows (needed only for higher roughness)
		if(u_passConstants.enableHitDistanceBasedMaxAge && centerPixelSurface.roughness >= 0.15f)
		{
			const float hitDist = u_historySpecularHitDist.Load(uint3(historySamplePixel, 0u));
			maxAge = Remap(hitDist, 0.8f, 4.0f, 4.f, float(u_passConstants.reservoirMaxAge));
		}
		else
		{
			maxAge = u_passConstants.reservoirMaxAge;
		}

		if(!m_wasSampleTraced)
		{
			maxAge *= 2.f;
		}

		maxAge = Remap(centerPixelSurface.roughness, 0.1f, 0.4f, 0.5f, 1.f) * maxAge;

		return maxAge;
	}

	bool TrySelectHistorySample(in int2 historySamplePixel, inout RngState rng)
	{
		const uint sampleIdx = m_currentSampleIdx++;

		m_sampleCoords[sampleIdx] = -1;
		if(all(historySamplePixel < 0) || all(historySamplePixel >= u_resamplingConstants.resolution))
		{
			return false;
		}

		m_sampleCoords[sampleIdx] = historySamplePixel;

		const MinimalSurfaceInfo selectedHistorySurface = GetMinimalSurfaceInfo(historyGBuffer, historySamplePixel, u_prevFrameSceneView);

		if(!SurfacesAllowResampling(centerPixelSurface, selectedHistorySurface)
			|| !MaterialsAllowResampling(centerPixelSurface, selectedHistorySurface))
		{
			return false;
		}

		m_foundValidHistorySurface = true;

		SRReservoir historyReservoir = LoadHistoryReservoir(historySamplePixel);

		if(!historyReservoir.IsValid() || !currentReservoir.CanCombine(historyReservoir))
		{
			return false;
		}

		currentReservoir.spatialResamplingRangeID = historyReservoir.spatialResamplingRangeID;

		const uint maxAge = ComputeReservoirMaxAge(historySamplePixel);

		if (historyReservoir.age > maxAge)
		{
			return false;
		}

		if(!IsReservoirValidForSurface(historyReservoir, centerPixelSurface))
		{
			return false;
		}

		const float jacobian = EvaluateJacobian(centerPixelSurface.location, selectedHistorySurface.location + centerPixelSurface.location - selectedHistorySurface.location, historyReservoir);

		if(jacobian <= 0.f)
		{
			return false;
		}

		currentReservoir.spatialResamplingRangeID = historyReservoir.spatialResamplingRangeID;

		if(!historyReservoir.HasValidResult())
		{
			return false;
		}

		historyReservoir.luminance = ExposedHistoryLuminanceToCurrentExposedLuminance(historyReservoir.luminance);

		const float p_hat = EvaluateTargetFunction(centerPixelSurface, historyReservoir.hitLocation, historyReservoir.luminance);
		const float p_hatInOutputDomain = p_hat * jacobian;

		if(isnan(p_hatInOutputDomain) || isinf(p_hatInOutputDomain) || IsNearlyZero(p_hatInOutputDomain))
		{
			return false;
		}

		m_validSamplesMask |= (1u << sampleIdx);

		if(currentReservoir.Update(historyReservoir, rng.Next(), p_hatInOutputDomain))
		{
			currentReservoir.RemoveFlag(SR_RESERVOIR_FLAGS_VALIDATED);
			m_selectedP_hat     = p_hat;
			m_selectedSampleIdx = sampleIdx;
		}

		return true;
	}

	SRReservoir FinishResampling(int2 pixel)
	{
		currentReservoir.age++;

		// MIS normalization based on equation 5.11 and 3.10 from A Gentle Introduction to Restir
		float p_i    = m_selectedSampleIdx >= 0 ? 0.f : m_selectedP_hat;
		float p_jSum = m_selectedP_hat * m_initialM; // initial sample

		// Input domains Ommega_i may overlap so we need to compute MIS weight for final sample
		// To do this, we map the sample to the input domains Omega_i
		// See: Generalized Resampled Importance Sampling: Foundations of ReSTIR
		[unroll]
		for(uint sampleIdx = 0u; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
		{
			if((m_validSamplesMask & (1u << sampleIdx)) == 0u)
			{
				continue;
			}
	
			const int2 samplePixel = m_sampleCoords[sampleIdx];
	
			const MinimalSurfaceInfo reusedSurface = GetMinimalSurfaceInfo(historyGBuffer, samplePixel, u_prevFrameSceneView);
	
			const SRReservoir reuseReservoir = LoadHistoryReservoir(samplePixel);
	
			// p_j in input domain Omega_j
			const float p_j = EvaluateTargetFunction(reusedSurface, currentReservoir.hitLocation, currentReservoir.luminance);
	
			p_jSum += p_j * reuseReservoir.M;
	
			if(sampleIdx == m_selectedSampleIdx)
			{
				p_i = p_j;
			}
		}
	
		currentReservoir.Normalize_BalancedHeuristic(m_selectedP_hat, p_i, p_jSum);

		return currentReservoir;
	}

	bool HasFoundValidHistorySurface()
	{
		return m_foundValidHistorySurface;
	}
};

#if ENABLE_SECOND_TRACING_PASS
void AppendAdditionalTraceCommand(in uint2 traceCoords)
{
	const uint commandsNum = WaveActiveCountBits(true);

	uint appendOffset = 0u;
	if (WaveIsFirstLane())
	{
		InterlockedAdd(u_commandsNum[0], commandsNum, OUT appendOffset);

		uint tracesNum = 0;
		InterlockedAdd(u_tracesNum[0], commandsNum, OUT tracesNum);
		InterlockedMax(u_tracesDispatchGroupsNum[0], (tracesNum + commandsNum + 63) / 64);
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


bool WasVariableRateReprojectionSuccessful(in uint2 coords)
{
	const uint2 reprojectionSuccessMaskCoords = coords << (u_passConstants.variableRateTileSizeBitOffset + uint2(3u, 2u));
	const uint reprojectionSuccessMaskBit     = (coords.y & 3u) * 8u + (coords.x & 7u);

	const uint reprojectionSuccessMask = u_vrReprojectionSuccessMask.Load(uint3(reprojectionSuccessMaskCoords, 0u)).x;
	const bool reprojectionSuccess     = (reprojectionSuccessMask & (1u << reprojectionSuccessMaskBit)) != 0u;

	return reprojectionSuccess;
}
#endif // ENABLE_SECOND_TRACING_PASS


[numthreads(8, 8, 1)]
void ResampleTemporallyCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

#if ENABLE_SECOND_TRACING_PASS
	if(all(pixel == 0))
	{
		u_commandsNum[1] = 1;
		u_commandsNum[2] = 1;
	}
#endif // ENABLE_SECOND_TRACING_PASS
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		const float depth = u_depthTexture.Load(uint3(pixel, 0));
		if(depth <= 0.f)
		{
			return;
		}

		MinimalGBuffer currentGBuffer;
		currentGBuffer.depthTexture             = u_depthTexture;
		currentGBuffer.normalsTexture           = u_normalsTexture;
		currentGBuffer.baseColorMetallicTexture = u_baseColorTexture;
		currentGBuffer.roughnessTexture         = u_roughnessTexture;

		const MinimalSurfaceInfo centerPixelSurface = GetMinimalSurfaceInfo(currentGBuffer, pixel, u_sceneView);

		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.reservoirsResolution);

		RngState rng = RngState::Create(pixel, u_resamplingConstants.frameIdx);

		SRTemporalResampler resampler;

		uint2 traceCoords;
		uint variableRateMask;
		GetVariableRateInfo(u_rwVariableRateBlocksTexture, pixel, OUT traceCoords, OUT variableRateMask);
		const bool wasSampleTraced = all(traceCoords == pixel);

		{
			const SRPackedReservoir packedReservoir = u_initialResservoirsBuffer[reservoirIdx];
			const SRReservoir reservoir = UnpackReservoir(packedReservoir);

			resampler = SRTemporalResampler::Create(centerPixelSurface, reservoir, wasSampleTraced);
		}

		if (centerPixelSurface.roughness >= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			const float2 uv = (float2(pixel) + 0.5f) * u_resamplingConstants.pixelSize;
			
			const float2 motion = u_motionTexture.Load(uint3(pixel, 0u));

			float2 reprojectedUV = uv - motion;

			if(all(reprojectedUV >= -0.1f) && all(reprojectedUV <= 1.1f))
			{
				// "jitter" the reprojected UV to break up any patterns (which are very undesirable in the denoiser) at the cost of noise
				const float randomRange = 48.f;
				if(reprojectedUV.x < 0.f)
				{
					reprojectedUV.x = rng.Next() * randomRange * u_resamplingConstants.pixelSize.x;
				}
				else if (reprojectedUV.x > 1.f)
				{
					reprojectedUV.x = 1.f - rng.Next() * randomRange * u_resamplingConstants.pixelSize.x;
				}

				if (reprojectedUV.y < 0.f)
				{
					reprojectedUV.y = rng.Next() * randomRange * u_resamplingConstants.pixelSize.y;
				}
				else if (reprojectedUV.y > 1.f)
				{
					reprojectedUV.y = 1.f - rng.Next() * randomRange * u_resamplingConstants.pixelSize.y;
				}

				for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
				{
					const float2 sampleUV = reprojectedUV - (u_sceneView.jitter - u_prevFrameSceneView.jitter) * sampleIdx;
					const int2 samplePixel = floor(sampleUV * u_resamplingConstants.resolution);
					
					if(resampler.TrySelectHistorySample(samplePixel, INOUT rng))
					{
						break;
					}
				}
			}
		}

		SRReservoir newReservoir = resampler.FinishResampling(pixel);

#if ENABLE_SECOND_TRACING_PASS
		const bool reprojectionSuccess = WasVariableRateReprojectionSuccessful(pixel);

		const uint invalidReservoirsNum = WaveActiveCountBits(!newReservoir.IsValid());
		const bool isValidReservoir     = newReservoir.IsValid() && any(newReservoir.weightSum * newReservoir.luminance) > 0.001f;

		if(reprojectionSuccess && !isValidReservoir)
		{
			AppendAdditionalTraceCommand(pixel);
		}
#endif // ENABLE_SECOND_TRACING_PASS

		if(variableRateMask <= SPT_VARIABLE_RATE_2X2 || (u_resamplingConstants.frameIdx & 3u) != ((pixel.x & 1u) + ((pixel.y & 1u) << 1u)))
		{
			newReservoir.AddFlag(SR_RESERVOIR_FLAGS_VALIDATED);
		}

		const SRPackedReservoir packedReservoir = PackReservoir(newReservoir);

		u_outReservoirsBuffer[reservoirIdx] = packedReservoir;

		if(!wasSampleTraced)
		{
			u_initialResservoirsBuffer[reservoirIdx] = packedReservoir;
		}
	}
}
