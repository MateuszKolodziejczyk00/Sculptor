#include "SculptorShader.hlsli"

[[descriptor_set(AccumulateVolumetricCloudsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

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

    const float tracedCloudDepth = u_tracedCloudsDepth.Load(uint3(coords >> 1u, 0u));

    if(isnan(tracedCloudDepth))
    {
        u_rwAccumulatedCloudsDepth[coords] = -1.f;
        u_rwCloudsAge[coords] = 0u;
        return;
    }

    const float2 uv = (coords + 0.5f) * u_passConstants.rcpResolution;

    const uint2 coordInTile = coords & 1u;

    const bool wasSampleTraced = all(coordInTile == u_passConstants.tracedPixel2x2);

    float reprojectionDepth = WaveActiveSum(tracedCloudDepth > 0.f ? tracedCloudDepth : 0.f) / WaveActiveCountBits(tracedCloudDepth > 0.f);
    if(isnan(reprojectionDepth))
    {
        reprojectionDepth = 1500.f;
    }

    const Ray viewRay = CreateViewRayWSNoJitter(u_sceneView, uv);
    const float3 reprjectedLocation = viewRay.GetTimeLocation(reprojectionDepth);

    const float2 lastFrameUV = WorldSpaceToNDCNoJitter(reprjectedLocation, u_prevFrameSceneView).xy * 0.5f + 0.5f;

    float4 outCloud = 0.f;
    float  outCloudDepth = 0.f;

    float4 tracedCloud = u_tracedClouds.Load(uint3(coords >> 1u, 0u));
    tracedCloud.xyz = ExposedLuminanceToLuminance(tracedCloud.xyz);

    uint historyAge = 0u;

    if(all(lastFrameUV >= 0.f) && all(lastFrameUV <= 1.f))
    {
        float4 historyCloud;
        if(all(abs(lastFrameUV - uv) < 0.001f))
        {
            historyCloud = u_accumulatedCloudsHistory.SampleLevel(u_linearSampler, lastFrameUV, 0.f);
        }
        else
        {
            historyCloud = SampleCatmullRom(u_accumulatedCloudsHistory, u_linearSampler, lastFrameUV, u_passConstants.resolution);
        }
        historyCloud.xyz = HistoryExposedLuminanceToLuminance(historyCloud.xyz);

        const float2 lastFrameUVFrac = frac(lastFrameUV * u_passConstants.resolution);

        float weight[4];
        weight[0] = (1.f - lastFrameUVFrac.x) * lastFrameUVFrac.y;
        weight[1] = (lastFrameUVFrac.x) * lastFrameUVFrac.y;
        weight[2] = lastFrameUVFrac.x * (1.f - lastFrameUVFrac.y);
        weight[3] = (1.f - lastFrameUVFrac.x) * (1.f - lastFrameUVFrac.y);

        const float4 historyCloudDepth4 = u_accumulatedCloudsDepthHistory.GatherRed(u_linearSampler, lastFrameUV, 0);

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
        historyAge = min(u_accumulatedCloudsAge.Load(uint3((lastFrameUV * u_passConstants.resolution), 0.f)), maxAge);

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
    
    if(historyAge == 0u)
    {
        outCloud      = tracedCloud;
        outCloudDepth = tracedCloudDepth;
    }

    outCloud.xyz = LuminanceToExposedLuminance(outCloud.xyz);
    u_rwAccumulatedClouds[coords]      = outCloud;
    u_rwAccumulatedCloudsDepth[coords] = outCloudDepth;

    uint newAge;
    newAge = historyAge + (wasSampleTraced ? 1u : 0u);
    
    u_rwCloudsAge[coords] = newAge;
}