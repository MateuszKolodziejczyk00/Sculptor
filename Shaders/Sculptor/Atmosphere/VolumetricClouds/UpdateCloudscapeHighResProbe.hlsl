#include "SculptorShader.hlsli"

[[descriptor_set(UpdateCloudscapeHighResProbeDS, 0)]]
[[descriptor_set(CloudscapeDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/SceneViewUtils.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void UpdateCloudscapeHighResProbeCS(CS_INPUT input)
{
    const uint2 coords = input.globalID.xy;
    const float2 uv = (input.globalID.xy + 0.5f) * u_cloudscapeConstants.highResProbeRcpRes;

	const float3 direction = OctahedronDecodeHemisphereNormal(uv);

    const float3 probeLocation = 0.f;

    const Ray ray = Ray::Create(probeLocation, direction);

    const float3 skyAvgLuminance = u_skyViewProbe.Load(0u);

    const float noise = frac(u_blueNoise256.Load(uint3(coords & 255u, 0u)) + SPT_GOLDEN_RATIO * (u_constants.frameIdx & 31u));

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = ray;
    raymarchParams.samplesNum = 64.f;
    raymarchParams.noise = noise;
    raymarchParams.ambient = skyAvgLuminance;
    const CloudscapeRaymarchResult raymarchRes = RaymarchCloudscape(raymarchParams);

    const float4 newValue = float4(raymarchRes.inScattering, raymarchRes.transmittance);
    const float4 oldValue = u_rwProbe[coords];

    const float weight = 0.1f;

    u_rwProbe[coords] = lerp(oldValue, newValue, weight);
}