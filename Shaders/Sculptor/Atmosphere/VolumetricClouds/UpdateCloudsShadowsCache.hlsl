#include "SculptorShader.hlsli"

[[descriptor_set(CloudscapeDS)]]

[[shader_params(CloudsShadowsCacheConstants, u_constants)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void UpdateCloudsShadowsCacheCS(CS_INPUT input)
{
    const uint3 coords = input.globalID.xyz + uint3(u_constants.updateOffset, 0u);

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

	const float3 uvw = float3(coords + 0.5f) * cloudscape.shadowsCacheVoxelSize;
	const float2 voxelLocation2D = cloudscape.shadowsCacheOrigin + uvw.xy * cloudscape.shadowsCacheSize;

	const float2 minMaxHeight = GetShadowsCacheMinMaxHeightAtLocation(voxelLocation2D);

	const float voxelLocationZ = minMaxHeight.x + uvw.z * (minMaxHeight.y - minMaxHeight.x);

	const float3 rayDirection = -cloudscape.mainDirectionalLight.direction;

	float tracedDistance;

	CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
	raymarchParams.ray         = Ray::Create(float3(voxelLocation2D, voxelLocationZ), rayDirection);
	raymarchParams.samplesNum  = u_constants.resetCache ? 256.f : 8.f;
	raymarchParams.maxDistance = u_constants.resetCache ? -1.f : 512.f;
	float transmittance = RaymarchCloudscapeTransmittance(raymarchParams, OUT tracedDistance);

	if (tracedDistance > 512.f * 0.99f && u_constants.prevShadowsCache.IsValid())
	{
		const float3 rayEnd = raymarchParams.ray.origin + rayDirection * tracedDistance;
		const float3 shadowCacheUVW = GetShadowCacheUVW(rayEnd);

		transmittance *= u_constants.prevShadowsCache.SampleLevel(u_cloadsLinearClampSampler, shadowCacheUVW, 0.f);
	}

	u_constants.rwShadowsCache.Store(coords, transmittance);
}
