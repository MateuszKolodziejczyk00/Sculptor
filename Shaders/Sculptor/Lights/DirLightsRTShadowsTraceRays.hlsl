#include "SculptorShader.hlsli"

[[descriptor_set(TraceShadowRaysDS, 2)]]
[[descriptor_set(DirectionalLightShadowMaskDS, 3)]]
[[descriptor_set(RenderViewDS, 4)]]

#include "Utils/BlueNoiseSamples.hlsli"
#include "Utils/RTVisibilityCommon.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Random.hlsli"


[shader("raygeneration")]
void GenerateShadowRaysRTG()
{
	const uint2 pixel = DispatchRaysIndex().xy;

	if(!u_params.enableShadows)
	{
		u_shadowMask[pixel] = 1.f;
		return;
	}
	
	const float2 uv = (pixel + 0.5f) / float2(DispatchRaysDimensions().xy);
	const float depth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0);

	RTVisibilityPayload payload = { false };

	if(depth > 0.f)
	{
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		float3 worldLocation = NDCToWorldSpaceNoJitter(ndc, u_sceneView);

		const float3 normal = u_normalsTexture.SampleLevel(u_nearestSampler, uv, 0) * 2.f - 1.f;

		if(dot(normal, u_params.lightDirection) <= 0.015f)
		{
			worldLocation += normal * u_params.shadowRayBias;

			const float maxConeAngle = u_params.shadowRayConeAngle;
			
			const uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x &15u) + u_gpuSceneFrameConstants.frameIdx * 23u)) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[sampleIdx]);
			const float3 shadowRayDirection = VectorInCone(-u_params.lightDirection, maxConeAngle, noise);

			RayDesc rayDesc;
			rayDesc.TMin        = u_params.minTraceDistance;
			rayDesc.TMax        = u_params.maxTraceDistance;
			rayDesc.Origin      = worldLocation;
			rayDesc.Direction   = shadowRayDirection;

			TraceRay(u_worldAccelerationStructure,
					 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
					 0xFF,
					 0,
					 1,
					 0,
					 rayDesc,
					 payload);
		}
		else
		{
			payload.isVisible = false;
		}
	}

	u_shadowMask[pixel] = payload.isVisible ? 1.f : 0.f;
}
