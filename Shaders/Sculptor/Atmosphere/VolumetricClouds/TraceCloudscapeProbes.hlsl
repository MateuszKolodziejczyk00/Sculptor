#include "SculptorShader.hlsli"

[[descriptor_set(TraceCloudscapeProbesDS, 0)]]
[[descriptor_set(CloudscapeDS, 1)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"

struct CS_INPUT
{
    uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void TraceCloudscapeProbesCS(CS_INPUT input)
{
    const uint probeIdx = input.groupID.y;
    const uint rayIdx   = input.groupID.x;

	const float3 direction = ComputeCloudscapeProbeRayDirection(rayIdx, u_constants.raysPerProbe);

    const uint2 probeCoords = u_constants.probesToUpdate[probeIdx].xy;

    const float3 probeLocation = GetCloudscapeProbeLocation(u_cloudscapeConstants, probeCoords);

    const Ray ray = Ray::Create(probeLocation, direction);

    const float3 skyAvgLuminance = u_skyViewProbe.Load(0u);

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = ray;
    raymarchParams.samplesNum = 64.f;
    raymarchParams.noise = 1.f;
    raymarchParams.ambient = skyAvgLuminance;
    raymarchParams.detailLevel = CLOUDS_DETAIL_PRESET_PROBE;
    const CloudscapeRaymarchResult raymarchRes = WaveRaymarchCloudscape<PROBE_CLODUD_SCATTERING_OCTAVES_NUM>(raymarchParams);

    if(WaveIsFirstLane())
    {
        u_rwTraceResult[uint2(rayIdx, probeIdx)] = float4(raymarchRes.inScattering, raymarchRes.transmittance);
    }
}