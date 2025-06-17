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
{
    const uint3 coords = uint3(input.globalID.xy, 0u);

    float2 uv;
    float  blueNoise;

    if(u_passConstants.fullResTrace)
    {
        uv = (coords.xy + 0.5f) * u_passConstants.rcpResolution;
        blueNoise = frac(u_blueNoise256.Load(coords & 255u) + ((u_passConstants.frameIdx) & 31u) * SPT_GOLDEN_RATIO);
    }
    else
    {
        uv = (coords.xy + 0.25f + 0.5f * u_passConstants.tracedPixel2x2) * u_passConstants.rcpResolution;
        blueNoise = frac(u_blueNoise256.Load((coords * 2u + uint3(u_passConstants.tracedPixel2x2, 0u)) & 255u) + ((u_passConstants.frameIdx >> 2u) & 31u) * SPT_GOLDEN_RATIO);
    }

    const float depth = u_furthestDepth.SampleLevel(u_depthSampler, uv, 0.f);

    const Ray viewRay = CreateViewRayWSNoJitter(u_sceneView, uv);

    const float3 skyAvgLuminance = u_skyProbe.Load(uint3(0u, 0u, 0u));

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = viewRay;
    raymarchParams.samplesNum = 128.f;
    raymarchParams.noise = blueNoise;
    raymarchParams.ambient = skyAvgLuminance;
    raymarchParams.maxVisibleDepth = depth > 0.f ? ComputeLinearDepth(depth, u_sceneView) : -1.f;
    raymarchParams.detailLevel = CLOUDS_DETAIL_PRESET_MAIN_VIEW;
    const CloudscapeRaymarchResult raymarchRes = RaymarchCloudscape<MAIN_VIEW_CLODUD_SCATTERING_OCTAVES_NUM>(raymarchParams);

    u_rwCloudsDepth[coords.xy] = raymarchRes.wasTraced ? raymarchRes.cloudDepth : SPT_NAN;

    const float3 exposedInScattering = LuminanceToExposedLuminance(raymarchRes.inScattering);

    u_rwClouds[coords.xy] = float4(exposedInScattering, raymarchRes.transmittance);
}