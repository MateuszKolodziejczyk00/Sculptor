#include "SculptorShader.hlsli"

[[descriptor_set(SRSpatialResamplingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
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


// Make sure that seed is same for whole warp, this will increase cache coherence
// Idea from: https://github.com/EmbarkStudios/kajiya/blob/main/assets/shaders/rtdgi/restir_spatial.hlsl
#define WAVE_COHERENT_SPATIAL_RESAMPLING 1


float3 ClipTargetLinDepthNDC(in float3 sourceLinDepthNDC, in float3 targetLinDepthNDC)
{
	const float3 d = targetLinDepthNDC - sourceLinDepthNDC;

	float maxT = 1.f;

	if(targetLinDepthNDC.x > 1.f)
	{
		maxT = min(maxT, (1.f - sourceLinDepthNDC.x) / d.x);
	}
	if(targetLinDepthNDC.y > 1.f)
	{
		maxT = min(maxT, (1.f - sourceLinDepthNDC.y) / d.y);
	}
	if(targetLinDepthNDC.x < -1.f)
	{
		maxT = min(maxT, (-1.f - sourceLinDepthNDC.x) / d.x);
	}
	if(targetLinDepthNDC.y < -1.f)
	{
		maxT = min(maxT, (-1.f - sourceLinDepthNDC.y) / d.y);
	}

	SPT_CHECK_MSG(maxT >= 0.f && maxT <= 1.f, L"Invalid clipping result");

	const float3 newTargetLinDepthNDC = sourceLinDepthNDC + d * maxT;

	SPT_CHECK_MSG(all(newTargetLinDepthNDC.xy >= -1.0001f), L"clipped end has coords lower than -1. New TargetLinDepthNDC: {}", newTargetLinDepthNDC);
	SPT_CHECK_MSG(all(newTargetLinDepthNDC.xy <= 1.0001f), L"clipped end has coords higher than 1 New TargetLinDepthNDC: {}", newTargetLinDepthNDC);

	return newTargetLinDepthNDC;
}


bool CoarseTestVisibility(in uint2 pixel, in float3 sourceWS, in float3 sourceNDC, in float3 targetWorldLocation)
{
	const float3 targetNDC = WorldSpaceToNDC(targetWorldLocation, u_sceneView);
	if(targetNDC.z >= 1.f || targetNDC.z <= 0.f) // target behind camera
	{
		return true;
	}

	float3 sourceLinDepthNDC = float3(sourceNDC.xy, 1.f / sourceNDC.z);
	float3 targetLinDepthNDC = float3(targetNDC.xy, 1.f / targetNDC.z);

	targetLinDepthNDC = ClipTargetLinDepthNDC(sourceLinDepthNDC, targetLinDepthNDC);

	const int2 sourceCoords = (sourceLinDepthNDC.xy * 0.5f + 0.5f) * u_resamplingConstants.resolution;
	const int2 targetCoords = (targetLinDepthNDC.xy * 0.5f + 0.5f) * u_resamplingConstants.resolution;
	const uint dist1d = MaxComponent(abs(targetCoords - sourceCoords));
	if(dist1d < 10u)
	{
		return true;
	}

	const uint maxStepsNum = 8u;

	const uint biasPixelsNum = max(4u, dist1d / maxStepsNum) + 1u;
	const float bias = biasPixelsNum / float(dist1d);

	const float3 biasLinDepthNDC = bias * (targetLinDepthNDC - sourceLinDepthNDC);
	sourceLinDepthNDC += biasLinDepthNDC;
	targetLinDepthNDC -= biasLinDepthNDC;

	const uint minStepsNum = 2u;
	const uint stepsNum = clamp((dist1d - (2u * biasPixelsNum)) / 2u, minStepsNum, maxStepsNum);

	const float3 step = (targetLinDepthNDC - sourceLinDepthNDC) / (stepsNum - 1u);

	const float rayDepthStep = abs(step.z) * GetNearPlane(u_sceneView);
	const float rayThickness = min(rayDepthStep, 0.1f) / rayDepthStep;

	float3 rayPos = sourceLinDepthNDC;

	for(uint i = 0u; i < stepsNum; ++i)
	{
		const float2 currentUV = rayPos.xy * 0.5f + 0.5f;
		const float sceneDepth = u_depthTexture.SampleLevel(u_nearestSampler, currentUV, 0.f);
		const float sceneInvDepth = rcp(sceneDepth);

		const float rayMax = rayPos.z;
		const float rayMin = rayPos.z - abs(step.z * rayThickness);

		if(sceneInvDepth >= rayMin && sceneInvDepth <= rayMax)
		{
			return false;
		}

		rayPos += step;
	}

	return true;
}


float ComputeResamplingRange(in const SRReservoir reservoir, in float roughness)
{
	const float stepSize = Remap(roughness, 0.1f, 0.4f, 0.4f, 1.f) * u_resamplingConstants.resamplingRangeStep;
	return max(reservoir.spatialResamplingRangeID * stepSize, 16.f);
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
	const float3 centerNDC = float3((pixel + 0.5f) / u_resamplingConstants.resolution * 2.f - 1.f, depth);

	if(depth <= 0.f)
	{
		return;
	}

	const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.reservoirsResolution);

	float selectedP_hat = 0.f;

	MinimalGBuffer gBuffer;
	gBuffer.depthTexture             = u_depthTexture;
	gBuffer.normalsTexture           = u_normalsTexture;
	gBuffer.baseColorMetallicTexture = u_baseColorTexture;
	gBuffer.roughnessTexture         = u_roughnessTexture;

	const MinimalSurfaceInfo centerPixelSurface = GetMinimalSurfaceInfo(gBuffer, pixel, u_sceneView);

	const SRPackedReservoir packedReservoir = u_inReservoirsBuffer[reservoirIdx];

	const SRReservoir reservoir = UnpackReservoir(packedReservoir);

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
		newReservoir.flags = reservoir.flags;

		const float p_hat = EvaluateTargetFunction(centerPixelSurface, reservoir.hitLocation, reservoir.luminance);

		newReservoir.Update(reservoir, 0.f, p_hat);

		newReservoir.spatialResamplingRangeID = reservoir.spatialResamplingRangeID;

		selectedP_hat = p_hat;
	}

	RngState rng = RngState::Create(uint2(pixel.x, pixel.y), u_passConstants.seed);

	int selectedSampleIdx = -1;
	int2 sampleCoords[SPATIAL_RESAMPLING_SAMPLES_NUM];
	uint validSamplesMask = 0u;

	float initialAngle = rng.Next() * 2 * PI;

#if WAVE_COHERENT_SPATIAL_RESAMPLING
	initialAngle = WaveReadLaneFirst(initialAngle);
#endif // WAVE_COHERENT_SPATIAL_RESAMPLING

	for(uint sampleIdx = 0; sampleIdx < SPATIAL_RESAMPLING_SAMPLES_NUM; ++sampleIdx)
	{
		const float sampleAngle = initialAngle + sampleIdx * (2 * PI / SPATIAL_RESAMPLING_SAMPLES_NUM);

		const float resamplingRange = u_passConstants.resamplingRangeMultiplier * ComputeResamplingRange(newReservoir, centerPixelSurface.roughness);

		const float cosAngle = cos(sampleAngle);
		const float sinAngle = sin(sampleAngle);

		float rangeMul = sqrt(rng.Next());

#if WAVE_COHERENT_SPATIAL_RESAMPLING
		rangeMul = WaveReadLaneFirst(rangeMul);
#endif // WAVE_COHERENT_SPATIAL_RESAMPLING

		const float distance = 1.5f + resamplingRange * rangeMul;
		float2 offset = float2(cosAngle, sinAngle) * distance;

		const int2 offsetInPixels = round(offset);

		int2 samplePixel = pixel + offsetInPixels;
		samplePixel = clamp(samplePixel, int2(0, 0), int2(u_resamplingConstants.resolution - 1));

		if (u_passConstants.resampleOnlyFromTracedPixels)
		{
			samplePixel = GetVariableTraceCoords(u_variableRateBlocksTexture, samplePixel);
		}

		sampleCoords[sampleIdx] = samplePixel;

		const MinimalSurfaceInfo reuseSurface = GetMinimalSurfaceInfo(gBuffer, samplePixel, u_sceneView);

		if(!SurfacesAllowResampling(centerPixelSurface, reuseSurface)
			|| !MaterialsAllowResampling(centerPixelSurface, reuseSurface))
		{
			newReservoir.OnSpatialResamplingFailed();
			continue;
		}

		const uint reuseReservoirIdx = GetScreenReservoirIdx(samplePixel, u_resamplingConstants.reservoirsResolution);
		SRReservoir reuseReservoir = UnpackReservoir(u_inReservoirsBuffer[reuseReservoirIdx]);

		if(!newReservoir.CanCombine(reuseReservoir))
		{
			newReservoir.OnSpatialResamplingFailed();
			continue;
		}

		if(!IsReservoirValidForSurface(reuseReservoir, centerPixelSurface))
		{
			newReservoir.OnSpatialResamplingFailed();
			continue;
		}

#if SPATIAL_RESAMPLING_ENABLE_SS_VISIBILITY
		if(!CoarseTestVisibility(pixel, centerPixelSurface.location, centerNDC, reuseReservoir.hitLocation))
		{
			newReservoir.OnSpatialResamplingFailed();
			continue;
		}
#endif // SPATIAL_RESAMPLING_ENABLE_SS_VISIBILITY

		const float jacobian = EvaluateJacobian(centerPixelSurface.location, reuseSurface.location, reuseReservoir);

		if(jacobian < 0.f)
		{
			newReservoir.OnSpatialResamplingFailed();
			continue;
		}

		const float p_hat = EvaluateTargetFunction(centerPixelSurface, reuseReservoir.hitLocation, reuseReservoir.luminance);
		const float p_hatInOutputDomain = p_hat * jacobian;

		if(isnan(p_hatInOutputDomain) || isinf(p_hatInOutputDomain) || IsNearlyZero(p_hatInOutputDomain))
		{
			newReservoir.OnSpatialResamplingFailed();
			continue;
		}

		newReservoir.OnSpatialResamplingSucceeded();

		validSamplesMask |= 1u << sampleIdx;

		if(newReservoir.Update(reuseReservoir, rng.Next(), p_hatInOutputDomain))
		{
			selectedP_hat = p_hat;
			newReservoir.RemoveFlag(SR_RESERVOIR_FLAGS_VALIDATED);
			selectedSampleIdx = sampleIdx;
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
