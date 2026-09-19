#include "SculptorShader.hlsli"

[[shader_params(VolumetricCloudsMainViewConstants, PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]

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

    if(PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->fullResTrace)
    {
        uv = (coords.xy + 0.5f) * PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->rcpResolution;
        blueNoise = frac(PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->blueNoise256.Load(coords & 255u) + ((PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->frameIdx) & 255u) * SPT_GOLDEN_RATIO);
    }
    else
    {
        uv = (coords.xy + 0.25f + 0.5f * PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->tracedPixel2x2) * PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->rcpResolution;
        blueNoise = frac(PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->blueNoise256.Load((coords * 2u + uint3(PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->tracedPixel2x2, 0u)) & 255u) + ((PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->frameIdx >> 2u) & 31u) * SPT_GOLDEN_RATIO);
    }

    const float depth = PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->furthestDepth.SampleLevel(BindlessSamplers::LinearMinClampEdge(), uv, 0.f);

    const Ray viewRay = CreateViewRayWSNoJitter(VIEW->sceneView, uv);

    const float3 skyAvgLuminance = PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->skyProbe.Load(uint3(0u, 0u, 0u));

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray = viewRay;
    raymarchParams.samplesNum = 128.f;
    raymarchParams.noise = blueNoise;
    raymarchParams.ambient = skyAvgLuminance;
    raymarchParams.maxVisibleDepth = depth > 0.f ? ComputeLinearDepth(depth, VIEW->sceneView) : -1.f;
    raymarchParams.detailLevel = CLOUDS_DETAIL_PRESET_MAIN_VIEW;
    const CloudscapeRaymarchResult raymarchRes = RaymarchCloudscape<MAIN_VIEW_CLODUD_SCATTERING_OCTAVES_NUM>(raymarchParams);

    PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->rwCloudsDepth[coords.xy] = raymarchRes.wasTraced ? raymarchRes.cloudDepth : SPT_NAN;

    const float3 exposedInScattering = LuminanceToExposedLuminance(raymarchRes.inScattering);

    PARAMS_RENDER_VOLUMETRIC_CLOUDS_MAIN_VIEW->rwClouds[coords.xy] = float4(exposedInScattering, raymarchRes.transmittance);
}
