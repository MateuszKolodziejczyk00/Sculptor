#include "SculptorShader.hlsli"

[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]

[[shader_params(CloudsShadowsCacheConstants, PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void UpdateCloudsShadowsCacheCS(CS_INPUT input)
{
    const uint3 coords = input.globalID.xyz + uint3(PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS->updateOffset, 0u);

    const CloudscapeConstants cloudscape = *PARAMS_CLOUDSCAPE;

	const float3 uvw = float3(coords + 0.5f) * cloudscape.shadowsCacheVoxelSize;
	const float2 voxelLocation2D = cloudscape.shadowsCacheOrigin + uvw.xy * cloudscape.shadowsCacheSize;

	const float2 minMaxHeight = GetShadowsCacheMinMaxHeightAtLocation(voxelLocation2D);

	const float voxelLocationZ = minMaxHeight.x + uvw.z * (minMaxHeight.y - minMaxHeight.x);

	const float3 rayDirection = -cloudscape.mainDirectionalLight.direction;

	float tracedDistance;

	CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
	raymarchParams.ray         = Ray::Create(float3(voxelLocation2D, voxelLocationZ), rayDirection);
	raymarchParams.samplesNum  = PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS->resetCache ? 256.f : 8.f;
	raymarchParams.maxDistance = PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS->resetCache ? -1.f : 512.f;
	float transmittance = RaymarchCloudscapeTransmittance(raymarchParams, OUT tracedDistance);

	if (tracedDistance > 512.f * 0.99f && PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS->prevShadowsCache.IsValid())
	{
		const float3 rayEnd = raymarchParams.ray.origin + rayDirection * tracedDistance;
		const float3 shadowCacheUVW = GetShadowCacheUVW(rayEnd);

		transmittance *= PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS->prevShadowsCache.SampleLevel(BindlessSamplers::LinearClampEdge(), shadowCacheUVW, 0.f);
	}

	PARAMS_CLOUDS_SHADOWS_CACHE_CONSTANTS->rwShadowsCache.Store(coords, transmittance);
}
