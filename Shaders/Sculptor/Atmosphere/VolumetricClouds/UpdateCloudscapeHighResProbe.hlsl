#include "SculptorShader.hlsli"

[[descriptor_set(UpdateCloudscapeHighResProbeDS, 0)]]
[[descriptor_set(CloudscapeDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/SceneViewUtils.hlsli"

struct CS_INPUT
{
    uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void UpdateCloudscapeHighResProbeCS(CS_INPUT input)
{
    const uint2 coords = u_constants.updateOffset + input.groupID.xy;

    const float2 uv = (coords + 0.5f) * u_cloudscapeConstants.highResProbeRcpRes;

    const float3 direction = OctahedronDecodeHemisphereNormal(uv);

    const float3 probeLocation = 0.f;

    const Ray ray = Ray::Create(probeLocation, direction);

    const float3 skyAvgLuminance = u_skyViewProbe.Load(0u);

    const float noise = frac(u_blueNoise256.Load(uint3(coords & 255u, 0u)) + SPT_GOLDEN_RATIO * (u_constants.updateLoopIdx & 31u));

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = ray;
    raymarchParams.samplesNum = 64.f;
    raymarchParams.noise = noise;
    raymarchParams.ambient = skyAvgLuminance;
    raymarchParams.detailLevel = CLOUDS_DETAIL_PRESET_PROBE;
    const CloudscapeRaymarchResult raymarchRes = WaveRaymarchCloudscape<PROBE_CLODUD_SCATTERING_OCTAVES_NUM>(raymarchParams);

    const float4 newValue = float4(raymarchRes.inScattering, raymarchRes.transmittance);
    const float4 oldValue = u_rwProbe[coords];

    const float weight = 0.1f;

    if(WaveIsFirstLane())
    {
        u_rwProbe[coords] = lerp(oldValue, newValue, weight);
    }
}