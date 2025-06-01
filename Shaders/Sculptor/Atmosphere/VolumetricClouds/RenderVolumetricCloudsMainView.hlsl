#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricCloudsMainViewDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(CloudscapeDS, 2)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RenderVolumetricCloudsMainViewCS(CS_INPUT input)
{    const uint3 coords = uint3(input.globalID.xy, 0u);

    const float depth = u_depth.Load(coords);

    if(depth > 0.f)
    {
        u_rwClouds[coords.xy] = float4(0.f, 0.f, 0.f, 1.f);
        return;
    }
    
    const float2 uv = (coords.xy + 0.5f) * u_passConstants.rcpResolution;

    const Ray viewRay = CreateViewRayWS(u_sceneView, uv);

    const float blueNoise = frac(u_blueNoise256.Load(coords & 255u) + (u_passConstants.frameIdx & 31u) * SPT_GOLDEN_RATIO);

    const float3 skyAvgLuminance = u_skyProbe.Load(uint3(0u, 0u, 0u));

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = viewRay;
    raymarchParams.samplesNum = 128.f;
    raymarchParams.noise = blueNoise;
    raymarchParams.ambient = skyAvgLuminance;
    const CloudscapeRaymarchResult raymarchRes = RaymarchCloudscape(raymarchParams);

    u_rwCloudsDepth[coords.xy] = raymarchRes.cloudDepth > 0.f ? ComputeProjectionDepth(raymarchRes.cloudDepth, u_sceneView) : 0.f;

    const float3 exposedInScattering = LuminanceToExposedLuminance(raymarchRes.inScattering);

    u_rwClouds[coords.xy] = float4(exposedInScattering, raymarchRes.transmittance);
}