#include "SculptorShader.hlsli"

[[descriptor_set(ApplyAtmosphereDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyAtmosphereCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_luminanceTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float depth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0);

		if(depth < 0.00001f)
		{
			const float3 rayDirection = ComputeViewRayDirectionWS(u_sceneView, uv);

			const float3 viewLocation = GetLocationInAtmosphere(u_atmosphereParams, u_sceneView.viewLocation);

			const float3 skyLuminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, viewLocation, rayDirection);

			float3 sunLuminance = float3(0.0f, 0.0f, 0.0f);

			for (int lightIdx = 0; lightIdx < u_atmosphereParams.directionalLightsNum; ++lightIdx)
			{
				const DirectionalLightGPUData directionalLight = u_directionalLights[lightIdx];
				const float3 lightDirection = -directionalLight.direction;	

				const float rayLightDot = dot(rayDirection, lightDirection);
				const float minRayLightDot = cos(directionalLight.sunDiskMinCosAngle);
				if (rayLightDot > minRayLightDot)
				{
					const float3 transmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, viewLocation, rayDirection);
					const float cosHalfApex = 0.01f;
					const float softEdge = saturate(2.0f * (rayLightDot - cosHalfApex) / (1.0f - cosHalfApex));
					sunLuminance += directionalLight.color * softEdge * transmittance * ComputeLuminanceFromEC(directionalLight.sunDiskEC);
				}
			}

			const float skyEyeAdaptationMultiplier = 2.7f;

			u_luminanceTexture[pixel]              = LuminanceToExposedLuminance(skyLuminance + sunLuminance);
			u_eyeAdaptationLuminanceTexture[pixel] = LuminanceToExposedLuminance(skyLuminance * skyEyeAdaptationMultiplier);
		}
	}
}
