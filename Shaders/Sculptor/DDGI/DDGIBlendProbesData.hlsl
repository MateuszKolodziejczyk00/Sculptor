#include "SculptorShader.hlsli"

#define DDGI_BLEND_ILLUMINANCE 1
#define DDGI_BLEND_DISTANCES 2


#define WEIGHT_EPSILON 0.0001f


#ifndef DDGI_BLEND_TYPE
#error "DDGI_BLEND_TYPE must be defined"
#endif // DDGI_BLEND_TYPE


#ifndef GROUP_SIZE_Y
#error "GROUP_SIZE_Y must be defined"
#endif // GROUP_SIZE_Y


#ifndef GROUP_SIZE_X
#error "GROUP_SIZE_X must be defined"
#endif // GROUP_SIZE_X


#ifndef RAYS_NUM_PER_PROBE
#error "RAYS_NUM_PER_PROBE must be defined"
#endif // RAYS_NUM_PER_PROBE


[[descriptor_set(DDGIBlendProbesDataDS, 0)]]

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

[[descriptor_set(DDGIUpdateProbesIlluminanceDS, 1)]]

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

[[descriptor_set(DDGIUpdateProbesHitDistanceDS, 1)]]

#endif // DDGI_BLEND_TYPE


groupshared float3 rayDirections[RAYS_NUM_PER_PROBE];

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

groupshared float4 rayResults[RAYS_NUM_PER_PROBE];

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

groupshared float rayHitDistances[RAYS_NUM_PER_PROBE];

#endif // DDGI_BLEND_TYPE

#include "DDGI/DDGItypes.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


void CacheRaysResults(in uint updatedProbeIdx, in uint2 threadLocalID)
{
    const uint flatThreadLocalID = threadLocalID.y * GROUP_SIZE_X + threadLocalID.x;

    uint2 traceResultTextureRes;
    u_traceRaysResultTexture.GetDimensions(traceResultTextureRes.x, traceResultTextureRes.y);

    const float2 rcpTraceResultTextureRes = rcp(float2(traceResultTextureRes));

    for (uint rayIdx = flatThreadLocalID; rayIdx < RAYS_NUM_PER_PROBE; rayIdx += (GROUP_SIZE_X * GROUP_SIZE_Y))
    {
        const float2 resultUV = (float2(updatedProbeIdx, rayIdx) + 0.5f) * rcpTraceResultTextureRes;
        const float4 rayResult = u_traceRaysResultTexture.SampleLevel(u_traceRaysResultSampler, resultUV, 0);

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

        rayResults[rayIdx] = rayResult;

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

        rayHitDistances[rayIdx] = rayResult.w;

#endif // DDGI_BLEND_TYPE

        rayDirections[rayIdx] = GetProbeRayDirection(rayIdx, RAYS_NUM_PER_PROBE);
    }
}

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void DDGIBlendProbesDataCS(CS_INPUT input)
{
    const uint updatedProbeIdx = input.groupID.x;

    const uint2 localID = input.localID.xy;

    CacheRaysResults(updatedProbeIdx, localID);

    GroupMemoryBarrierWithGroupSync();

    const uint3 updatedProbeCoords = ComputeUpdatedProbeCoords(updatedProbeIdx, u_relitParams.probesToUpdateCoords, u_relitParams.probesToUpdateCount);
    const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(u_volumeParams, updatedProbeCoords);

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

    const DDGIProbeDataCoords probeDataOffset = ComputeProbeIlluminanceDataOffset(u_volumeParams, probeWrappedCoords);
    const RWTexture2D<float4> probeIlluminanceTexture = u_volumeIlluminanceTextures[probeDataOffset.textureIdx];

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

    const DDGIProbeDataCoords probeDataOffset = ComputeProbeHitDistanceDataOffset(u_volumeParams, probeWrappedCoords);
    const RWTexture2D<float4> probeHitDistanceTexture = u_volumeHitDistanceTextures[probeDataOffset.textureIdx];

#endif // DDGI_BLEND_TYPE

    const bool isCorner = (localID.x == 0 || localID.x == GROUP_SIZE_X - 1) && (localID.y == 0 || localID.y == GROUP_SIZE_Y - 1);
    const bool isBorder = (localID.x == 0 || localID.x == GROUP_SIZE_X - 1 || localID.y == 0 || localID.y == GROUP_SIZE_Y - 1) && !isCorner;

    if(!isCorner && !isBorder)
    {
        const float2 sampleProbeUV = (float2(input.localID.xy) - 0.5f) / float2(GROUP_SIZE_X - 2, GROUP_SIZE_Y - 2);
        const float3 sampleDirection = GetProbeOctDirection(sampleProbeUV);

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

        float3 luminanceSum = 0.f;

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

        float hitDistanceSum           = 0.f;
        float hitDistanceSquaredSum    = 0.f;

#endif // DDGI_BLEND_TYPE
    
        float weightSum = 0.f;

        const uint maxBackfaces = 0.1f * RAYS_NUM_PER_PROBE;
        uint backfacesNum = 0;


        for (uint rayIdx = 0; rayIdx < RAYS_NUM_PER_PROBE; ++rayIdx)
        {
            float weight = max(dot(sampleDirection, rayDirections[rayIdx]), 0.f);

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

            float3 rayLuminance         = rayResults[rayIdx].xyz;
            const float rayHitDistance  = rayResults[rayIdx].w;

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

            float rayHitDistance = rayHitDistances[rayIdx];

#endif // DDGI_BLEND_TYPE

            if (rayHitDistance < 0.f)
            {
                ++backfacesNum;

                if (backfacesNum >= maxBackfaces)
                {
#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

                    probeIlluminanceTexture[probeDataOffset.textureLocalCoords + localID] = 0.f;

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

                    probeHitDistanceTexture[probeDataOffset.textureLocalCoords + localID] = -1.f;

#endif // DDGI_BLEND_TYPE
                    return;
                }

                continue;
            }
            
#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

            if(weight > WEIGHT_EPSILON)
            {
                luminanceSum += rayLuminance * weight;
                weightSum += weight;
            }

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

            weight = pow(weight, 128);

            if(weight > WEIGHT_EPSILON)
            {
                const float rayHitDistanceSquared = Pow2(rayHitDistance);

                hitDistanceSum          += rayHitDistance * weight;
                hitDistanceSquaredSum   += rayHitDistanceSquared * weight;
                weightSum               += weight;
            }

#endif // DDGI_BLEND_TYPE
        }

        const uint2 dataCoords = probeDataOffset.textureLocalCoords + localID;

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

        float3 result = 0.f;
        if(weightSum > WEIGHT_EPSILON)
        {
            // multiply by 2 because weightSum divide by sum of cosines instead of number of samples which should be used for monte carlo integration
            // this reduces variance but we must compensate it because its expected value is samplesNum / 2
            result = luminanceSum / (2.f * weightSum);
        }

        const float exponent = 1.f / u_volumeParams.probeIlluminanceEncodingGamma;
        result = pow(result, exponent);
        
        const float3 prevIlluminance = probeIlluminanceTexture[dataCoords].xyz;

        float hysteresis = u_relitParams.blendHysteresis;

        if (all(prevIlluminance) < 0.0001f)
        {
            hysteresis = 0.f;
        }
        
        float3 delta = result - prevIlluminance;

        const float maxIlluminanceDiff = abs(MaxComponent(result - prevIlluminance));
        const float histeresisDelta = maxIlluminanceDiff > u_relitParams.illuminanceDiffThreshold ? 0.5f : 0.f;
        hysteresis = max(hysteresis - histeresisDelta, 0.f);

        const float3 probeLocation = GetProbeWorldLocation(u_volumeParams, updatedProbeCoords);
		if (hysteresis > 0.0001f)
        {
            const float deltaLuminance = Luminance(abs(delta));
            if(deltaLuminance > u_relitParams.luminanceDiffThreshold)
            {
                delta *= 0.2f;
            }
        }

        float3 deltaThisFrame = delta * (1.f - hysteresis);
        result = prevIlluminance + deltaThisFrame;

        probeIlluminanceTexture[dataCoords] = float4(result, 0.f);

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

        float2 result = 0.f;
        if(weightSum > WEIGHT_EPSILON)
        {
            result = float2(hitDistanceSum, hitDistanceSquaredSum) / weightSum;
        }
        
        float hysteresis = u_relitParams.blendHysteresis;
        const float3 probeLocation = GetProbeWorldLocation(u_volumeParams, updatedProbeCoords);
        if (any(probeLocation < u_relitParams.prevAABBMin) || any(probeLocation > u_relitParams.prevAABBMax))
        {
            hysteresis = 0.f;
        }
        
        const float2 prevHitDistance = probeHitDistanceTexture[dataCoords].xy;

        if(prevHitDistance.x < 0.f)
        {
            hysteresis = 0.f;
        }

        result = lerp(result, prevHitDistance, hysteresis);
        
        probeHitDistanceTexture[dataCoords] = float4(result, 0.f, 0.f);

#endif // DDGI_BLEND_TYPE

        return;
    }

    AllMemoryBarrierWithGroupSync();

    const bool isRow = (localID.y == 0 || localID.y == GROUP_SIZE_Y - 1) && !isCorner;

    uint2 sourcePixelCoord = 0;

    if(isCorner)
    {
        if(localID.x != 0)
        {
            sourcePixelCoord.x = 1;
        }
        else
        {
            sourcePixelCoord.x = GROUP_SIZE_X - 2;
        }
        
        if(localID.y != 0)
        {
            sourcePixelCoord.y = 1;
        }
        else
        {
             sourcePixelCoord.y = GROUP_SIZE_Y - 2;
        }
    }
    else if(isRow)
    {
        sourcePixelCoord.x = GROUP_SIZE_X - 2 - localID.x;
        
         if(localID.y != 0)
        {
            sourcePixelCoord.y = GROUP_SIZE_Y - 2;
        }
        else
        {
            sourcePixelCoord.y = 1;
        }
    }
    else
    {
        if(localID.x != 0)
        {
            sourcePixelCoord.x = GROUP_SIZE_X - 2;
        }
        else
        {
            sourcePixelCoord.x = 1;
        }

        sourcePixelCoord.y = GROUP_SIZE_Y - 2 - localID.y;
    }

#if DDGI_BLEND_TYPE == DDGI_BLEND_ILLUMINANCE

    probeIlluminanceTexture[probeDataOffset.textureLocalCoords + localID] = probeIlluminanceTexture[probeDataOffset.textureLocalCoords + sourcePixelCoord];

#elif DDGI_BLEND_TYPE == DDGI_BLEND_DISTANCES

    probeHitDistanceTexture[probeDataOffset.textureLocalCoords + localID] = probeHitDistanceTexture[probeDataOffset.textureLocalCoords + sourcePixelCoord];

#endif // DDGI_BLEND_TYPE
}
