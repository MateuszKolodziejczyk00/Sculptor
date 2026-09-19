#include "SculptorShader.hlsli"

[[shader_params(TraceCloudscapeProbesConsts, PARAMS_TRACE_CLOUDSCAPE_PROBES)]]
[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]

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

	const float3 direction = ComputeCloudscapeProbeRayDirection(rayIdx, PARAMS_TRACE_CLOUDSCAPE_PROBES->constants.raysPerProbe);

    const uint2 probesNum = PARAMS_CLOUDSCAPE->probesNum;

#if FULL_UPDATE
    const uint2 probeCoords = uint2(probeIdx % probesNum.x, probeIdx / probesNum.x);
#else
    const uint2 probeCoords = PARAMS_TRACE_CLOUDSCAPE_PROBES->constants.probesToUpdate[probeIdx].xy;
#endif // FULL_UPDATE

    const float3 probeLocation = GetCloudscapeProbeLocation(*PARAMS_CLOUDSCAPE, probeCoords);

    const Ray ray = Ray::Create(probeLocation, direction);

    const float3 skyAvgLuminance = PARAMS_TRACE_CLOUDSCAPE_PROBES->skyViewProbe.Load(0u);

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = ray;
    raymarchParams.samplesNum = 64.f;
    raymarchParams.noise = 1.f;
    raymarchParams.ambient = skyAvgLuminance;
    raymarchParams.detailLevel = CLOUDS_DETAIL_PRESET_PROBE;
    const CloudscapeRaymarchResult raymarchRes = WaveRaymarchCloudscape<PROBE_CLODUD_SCATTERING_OCTAVES_NUM>(raymarchParams);

    if(WaveIsFirstLane())
    {
        PARAMS_TRACE_CLOUDSCAPE_PROBES->rwTraceResult[uint2(rayIdx, probeIdx)] = float4(raymarchRes.inScattering, raymarchRes.transmittance);
    }
}
