#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeConstants, CLOUDSCAPE)]]
[[shader_params(UpdateCloudscapeHighResProbeConstants, CONSTS)]]

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
    const uint2 coords = CONSTS->updateOffset + input.groupID.xy;

    const float2 uv = (coords + 0.5f) * CLOUDSCAPE->highResProbeRcpRes;

    const float3 direction = OctahedronDecodeHemisphereNormal(uv);

    const float3 probeLocation = 0.f;

    const Ray ray = Ray::Create(probeLocation, direction);

    const float3 skyAvgLuminance = CONSTS->skyViewProbe.Load(0u);

    const float noise = frac(GLOBALS->blueNoise256.Load(uint3(coords & 255u, 0u)) + SPT_GOLDEN_RATIO * (CONSTS->updateLoopIdx & 31u));

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = ray;
    raymarchParams.samplesNum = 64.f;
    raymarchParams.noise = noise;
    raymarchParams.ambient = skyAvgLuminance;
    raymarchParams.detailLevel = CLOUDS_DETAIL_PRESET_PROBE;
    const CloudscapeRaymarchResult raymarchRes = WaveRaymarchCloudscape<PROBE_CLODUD_SCATTERING_OCTAVES_NUM>(raymarchParams);

    const float4 newValue = float4(raymarchRes.inScattering, raymarchRes.transmittance);
    const float4 oldValue = CONSTS->rwProbe[coords];

    if(WaveIsFirstLane())
    {
        CONSTS->rwProbe[coords] = CONSTS->blendFactor < 1.f ? lerp(oldValue, newValue, CONSTS->blendFactor) : newValue;
    }
}
