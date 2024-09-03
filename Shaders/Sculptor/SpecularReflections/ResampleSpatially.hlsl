#include "SculptorShader.hlsli"

[[descriptor_set(SRSpatialResamplingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SRResamplingCommon.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/MortonCode.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};


#define SPATIAL_RESAMPLING_SAMPLES_NUM 2


float ComputeResamplingRange(in float roughness, in float accumulatedSamplesNum)
{
	return 32.f;
}


groupshared float gs_reservoirWeightSum[2];
groupshared float gs_validReservoirsNum[2];


void ExecuteFireflyFilter(in uint threadIdx, inout SRReservoir reservoir)
{
	const float reservoirWeight = reservoir.IsValid() ? Luminance(reservoir.luminance) * reservoir.weightSum : 0.f;

	const float reservoirWeightSum = WaveActiveSum(reservoirWeight);
	const uint validReservoirsNum  = WaveActiveCountBits(reservoir.IsValid());

	const uint waveIdx = threadIdx / WaveGetLaneCount();

	gs_reservoirWeightSum[waveIdx] = reservoirWeightSum;
	gs_validReservoirsNum[waveIdx] = validReservoirsNum;

	GroupMemoryBarrierWithGroupSync();

	const float avgWeight = (gs_reservoirWeightSum[0] + gs_reservoirWeightSum[1]) / max(gs_validReservoirsNum[0] + gs_validReservoirsNum[1], 1u);

	const float maxWeight = avgWeight * 12.f;

	if(reservoirWeight > maxWeight)
	{
		reservoir.weightSum = 0.f;
	}
}


[numthreads(64, 1, 1)]
void ResampleSpatiallyCS(CS_INPUT input)
{
	const uint localThreadID = input.localID.x;
	const uint2 localID = DecodeMorton2D(localThreadID);

	uint2 pixel = input.groupID.xy * uint2(8u, 8u) + localID;

	bool isHelperLane = false;
	if(any(pixel >= u_resamplingConstants.resolution))
	{
		isHelperLane = true;
		pixel = min(pixel, u_resamplingConstants.resolution - 1u);
	}
	
	const float depth = u_depthTexture.Load(uint3(pixel, 0)).x;

	if(depth <= 0.f)
	{
		return;
	}

	const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.reservoirsResolution);

	float selectedP_hat = 0.f;

	MinimalGBuffer gBuffer;
	gBuffer.depthTexture         = u_depthTexture;
	gBuffer.normalsTexture       = u_normalsTexture;
	gBuffer.specularColorTexture = u_specularColorTexture;
	gBuffer.roughnessTexture     = u_roughnessTexture;

	const MinimalSurfaceInfo centerPixelSurface = GetMinimalSurfaceInfo(gBuffer, pixel, u_sceneView);

	const SRPackedReservoir packedReservoir = u_inReservoirsBuffer[reservoirIdx];

	SRReservoir reservoir = UnpackReservoir(packedReservoir);
	ExecuteFireflyFilter(localThreadID, reservoir); // Must be before early out for roughness, it relies on wave intrinsics and all lanes being active

	if(WaveActiveAllTrue(isHelperLane))
	{
		return;
	}

	if(centerPixelSurface.roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		u_outReservoirsBuffer[reservoirIdx] = packedReservoir;
		return;
	}

	SRReservoir newReservoir = SRReservoir::CreateEmpty();
	{
		const float p_hat = EvaluateTargetFunction(centerPixelSurface, reservoir.hitLocation, reservoir.luminance);

		newReservoir.Update(reservoir, 0.f, p_hat);

		selectedP_hat = p_hat;
	}

	RngState rng = RngState::Create(pixel, u_resamplingConstants.frameIdx);

	const float resamplingRange = ComputeResamplingRange(centerPixelSurface.roughness, newReservoir.age);

	int selectedSampleIdx = -1;
	int2 sampleCoords[SPATIAL_RESAMPLING_SAMPLES_NUM];
	uint validSamplesMask = 0u;

	for(uint sampleIdx = 0; sampleIdx < SPATIAL_RESAMPLING_SAMPLES_NUM; ++sampleIdx)
	{
		const float2 offset = -resamplingRange + 2 * resamplingRange * float2(rng.Next(), rng.Next());

		int2 offsetInPixels = round(offset);
		if(all(offsetInPixels <= 0))
		{
			if(abs(offsetInPixels.x) > abs(offsetInPixels.y))
			{
				offsetInPixels.x = SignNotZero(offset.x);
			}
			else
			{
				offsetInPixels.y = SignNotZero(offset.y);
			}
		}
		const int2 samplePixel = GetVariableTraceCoords(u_variableRateBlocksTexture, pixel + offset);

		sampleCoords[sampleIdx] = -1;

		if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
		{
			sampleCoords[sampleIdx] = samplePixel;

			const MinimalSurfaceInfo reuseSurface = GetMinimalSurfaceInfo(gBuffer, samplePixel, u_sceneView);

			if(!SurfacesAllowResampling(centerPixelSurface, reuseSurface)
				|| !MaterialsAllowResampling(centerPixelSurface, reuseSurface))
			{
				continue;
			}

			const uint reuseReservoirIdx = GetScreenReservoirIdx(samplePixel, u_resamplingConstants.reservoirsResolution);
			SRReservoir reuseReservoir = UnpackReservoir(u_inReservoirsBuffer[reuseReservoirIdx]);

			if(!newReservoir.CanCombine(reuseReservoir))
			{
				continue;
			}

			if(!IsReservoirValidForSurface(reuseReservoir, centerPixelSurface))
			{
				continue;
			}

			const float jacobian = EvaluateJacobian(centerPixelSurface.location, reuseSurface.location, reuseReservoir);

			if(jacobian < 0.f)
			{
				continue;
			}

			const float p_hat = EvaluateTargetFunction(centerPixelSurface, reuseReservoir.hitLocation, reuseReservoir.luminance);
			const float p_hatInOutputDomain = p_hat * jacobian;

			if(isnan(p_hatInOutputDomain) || isinf(p_hatInOutputDomain) || IsNearlyZero(p_hatInOutputDomain))
			{
				continue;
			}

			validSamplesMask |= 1u << sampleIdx;

			if(newReservoir.Update(reuseReservoir, rng.Next(), p_hatInOutputDomain))
			{
				selectedP_hat = p_hat;
				newReservoir.RemoveFlag(SR_RESERVOIR_FLAGS_RECENT);
				selectedSampleIdx = sampleIdx;
			}
		}
	}

	// MIS normalization based on equation 5.11 and 3.10 from A Gentle Introduction to Restir
	float p_i = selectedSampleIdx == -1 ? selectedP_hat : 0.f;
	float p_jSum = selectedP_hat * reservoir.M; // center sample

	// Input domains Ommega_i may overlap so we need to compute MIS weight for final sample
	// To do this, we map the sample to the input domains Omega_i
	// See: Generalized Resampled Importance Sampling: Foundations of ReSTIR
	[unroll]
	for(uint sampleIdx = 0; sampleIdx < SPATIAL_RESAMPLING_SAMPLES_NUM; ++sampleIdx)
	{
		if((validSamplesMask & (1u << sampleIdx)) == 0u)
		{
			continue;
		}

		const int2 samplePixel = sampleCoords[sampleIdx];

		const MinimalSurfaceInfo reusedSurface = GetMinimalSurfaceInfo(gBuffer, samplePixel, u_sceneView);

		const uint reuseReservoirIdx = GetScreenReservoirIdx(samplePixel, u_resamplingConstants.reservoirsResolution);
		SRReservoir reuseReservoir = UnpackReservoir(u_inReservoirsBuffer[reuseReservoirIdx]);

		// p_j in input domain Omega_j
		const float p_j = EvaluateTargetFunction(reusedSurface, newReservoir.hitLocation, newReservoir.luminance);

		p_jSum += p_j * reuseReservoir.M;

		if(sampleIdx == selectedSampleIdx)
		{
			p_i = p_j;
		}
	}

	newReservoir.Normalize_BalancedHeuristic(selectedP_hat, p_i, p_jSum);

	if(!isHelperLane)
	{
		u_outReservoirsBuffer[reservoirIdx] = PackReservoir(newReservoir);
	}
} 