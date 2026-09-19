#include "SculptorShader.hlsli"

[[shader_params(VolumetricCloudsAccumulateConstants, PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Sampling.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void AccumulateVolumetricCloudsCS(CS_INPUT input)
{
    const uint2 coords = input.globalID.xy;

    const float tracedCloudDepth = PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->tracedCloudsDepth.Load(uint3(coords >> 1u, 0u));

    if(isnan(tracedCloudDepth))
    {
        PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rwAccumulatedCloudsDepth[coords] = -1.f;
        PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rwCloudsAge[coords] = 0u;
		PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rwAccumulatedClouds[coords] = SPT_NAN;
        return;
    }

    const float2 uv = (coords + 0.5f) * PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rcpResolution;

    const uint2 coordInTile = coords & 1u;

    const bool wasSampleTraced = all(coordInTile == PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->tracedPixel2x2);

    float reprojectionDepth = WaveActiveSum(tracedCloudDepth > 0.f ? tracedCloudDepth : 0.f) / WaveActiveCountBits(tracedCloudDepth > 0.f);
    if(isnan(reprojectionDepth))
    {
        reprojectionDepth = 1500.f;
    }

    const Ray viewRay = CreateViewRayWSNoJitter(VIEW->sceneView, uv);
    const float3 reprjectedLocation = viewRay.GetTimeLocation(reprojectionDepth);

    const float2 lastFrameUV = WorldSpaceToNDCNoJitter(reprjectedLocation, VIEW->prevFrameSceneView).xy * 0.5f + 0.5f;

    float4 outCloud = 0.f;
    float  outCloudDepth = 0.f;

    float4 tracedCloud = PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->tracedClouds.Load(uint3(coords >> 1u, 0u));
    tracedCloud.xyz = ExposedLuminanceToLuminance(tracedCloud.xyz);

    uint historyAge = 0u;

    if(all(lastFrameUV >= 0.f) && all(lastFrameUV <= 1.f))
    {
        float4 historyCloud;
        // if(all(abs(lastFrameUV - uv) < 0.001f))
        // {
            historyCloud = PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->accumulatedCloudsHistory.SampleLevel(BindlessSamplers::LinearClampEdge(), lastFrameUV, 0.f);
        //}
        //else
        //{
        //    historyCloud = SampleCatmullRom(PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->accumulatedCloudsHistory, BindlessSamplers::LinearClampEdge(), lastFrameUV, PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->resolution);
        //}

        historyCloud.xyz = HistoryExposedLuminanceToLuminance(historyCloud.xyz);

        const float2 lastFrameUVFrac = frac(lastFrameUV * PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->resolution);

        float weight[4];
        weight[0] = (1.f - lastFrameUVFrac.x) * lastFrameUVFrac.y;
        weight[1] = (lastFrameUVFrac.x) * lastFrameUVFrac.y;
        weight[2] = lastFrameUVFrac.x * (1.f - lastFrameUVFrac.y);
        weight[3] = (1.f - lastFrameUVFrac.x) * (1.f - lastFrameUVFrac.y);

        const float4 historyCloudDepth4 = PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->accumulatedCloudsDepthHistory.GatherRed(BindlessSamplers::LinearClampEdge(), lastFrameUV);

        // We take into account only valid depths here, others are ignored. This can "blur" depth into regions on screen without clouds but it shouldn't be a problem since it's not used there at all
        float historyCloudDepth = 0.f;
        float depthWeightSum = 0.f;
        for(uint i = 0u; i < 4u; ++i)
        {
            if(historyCloudDepth4[i] > 0.f)
            {
                historyCloudDepth += historyCloudDepth4[i] * weight[i];
                depthWeightSum += weight[i];
            }
        }

        historyCloudDepth = depthWeightSum > 0.f ? historyCloudDepth / depthWeightSum : -1.f;

        const uint maxAge = distance(uv, lastFrameUV) < 0.001f ? 25u : 5u;
        historyAge = min(PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->accumulatedCloudsAge.Load(uint3((lastFrameUV * PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->resolution), 0.f)), maxAge);

        if(historyAge > 0u)
        {
            if(wasSampleTraced)
            {
                const float currentFrameWeight = 1.f / float(historyAge + 1u);

                outCloud      = lerp(historyCloud, tracedCloud, currentFrameWeight);
                outCloudDepth = tracedCloudDepth;
            }
            else
            {
                outCloud      = historyCloud;
                outCloudDepth = historyCloudDepth;
            }
        }
    }
    
    if(historyAge == 0u || any(isnan(outCloud)))
    {
        outCloud      = tracedCloud;
        outCloudDepth = tracedCloudDepth;
    }

    outCloud.xyz = LuminanceToExposedLuminance(outCloud.xyz);
    PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rwAccumulatedClouds[coords]      = outCloud;
    PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rwAccumulatedCloudsDepth[coords] = outCloudDepth;

    uint newAge;
    newAge = historyAge + (wasSampleTraced ? 1u : 0u);
    
    PARAMS_ACCUMULATE_VOLUMETRIC_CLOUDS->rwCloudsAge[coords] = newAge;
}
