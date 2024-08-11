#include "SculptorShader.hlsli"

[[descriptor_set(SRSpatialResamplingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SRResamplingCommon.hlsli"
#include "Utils/Random.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define SPATIAL_RESAMPLING_SAMPLES_NUM 2


float ComputeResamplingRange(in float roughness, in float accumulatedSamplesNum)
{
	return 32.f;
}


[numthreads(8, 4, 1)]
void ResampleSpatiallyCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.reservoirsResolution);

		float selectedTargetPdf = 0.f;

		MinimalGBuffer gBuffer;
		gBuffer.depthTexture         = u_depthTexture;
		gBuffer.normalsTexture       = u_normalsTexture;
		gBuffer.specularColorTexture = u_specularColorTexture;
		gBuffer.roughnessTexture     = u_roughnessTexture;

		const MinimalSurfaceInfo centerPixelSurface = GetMinimalSurfaceInfo(gBuffer, pixel, u_sceneView);

		const SRPackedReservoir packedReservoir = u_inReservoirsBuffer[reservoirIdx];

		if(centerPixelSurface.roughness <= 0.01f)
		{
			u_outReservoirsBuffer[reservoirIdx] = packedReservoir;
			return;
		}
		const SRReservoir reservoir = UnpackReservoir(packedReservoir);

		SRReservoir newReservoir = SRReservoir::CreateEmpty();
		{
			const float targetPdf = EvaluateTargetPdf(centerPixelSurface, reservoir.hitLocation, reservoir.luminance);

			newReservoir.Update(reservoir, 0.f, targetPdf);

			selectedTargetPdf = targetPdf;
		}

		RngState rng = RngState::Create(pixel, u_resamplingConstants.frameIdx);

		const float resamplingRange = ComputeResamplingRange(centerPixelSurface.roughness, newReservoir.age);

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

			if(all(samplePixel >= 0) && all(samplePixel < u_resamplingConstants.resolution))
			{
				const MinimalSurfaceInfo reuseSurface = GetMinimalSurfaceInfo(gBuffer, samplePixel, u_sceneView);

				if(!AreSurfacesSimilar(centerPixelSurface, reuseSurface)
					|| !AreMaterialsSimilar(centerPixelSurface, reuseSurface))
				{
					continue;
				}

				const uint reuseReservoirIdx = GetScreenReservoirIdx(samplePixel, u_resamplingConstants.reservoirsResolution);
				const SRReservoir reuseReservoir = UnpackReservoir(u_inReservoirsBuffer[reuseReservoirIdx]);

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

				const float targetPdf = EvaluateTargetPdf(centerPixelSurface, reuseReservoir.hitLocation, reuseReservoir.luminance);
				if(isnan(targetPdf) || isinf(targetPdf) || IsNearlyZero(targetPdf * jacobian))
				{
					continue;
				}

				if(newReservoir.Update(reuseReservoir, rng.Next(), targetPdf * jacobian))
				{
					selectedTargetPdf = targetPdf;
					newReservoir.RemoveFlag(SR_RESERVOIR_FLAGS_RECENT);
				}
			}
		}

		newReservoir.Normalize(selectedTargetPdf);

		u_outReservoirsBuffer[reservoirIdx] = PackReservoir(newReservoir);
	}
} 
