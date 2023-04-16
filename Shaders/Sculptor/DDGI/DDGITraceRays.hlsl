
#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DDGITraceRaysDS, 0)]]


#include "DDGI/DDGITypes.hlsli"


struct DDGIRayPayload
{
    float3 radiance;
    float hitDstance;
};


[shader("raygeneration")]
void DDGIProbeRaysRTG()
{
    const uint2 dispatchIdx = DispatchRaysIndex().xy;
    
    const uint rayIdx = dispatchIdx.y;
    const uint3 probeCoords = ComputeUpdatedProbeCoords(dispatchIdx.x, u_updateProbesParams.probesToUpdateCoords, u_updateProbesParams.probesToUpdateCount);
    const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(u_ddgiParams, probeCoords);

    const float3 rayDirection = FibbonaciSphereDistribution(rayIdx, u_updateProbesParams.raysNumPerProbe);

    const float3 probeWorldLocation = GetProbeWorldLocation(u_ddgiParams, probeWrappedCoords);

    DDGIRayPayload payload;
    payload.radiance = 1.f;
    payload.hitDstance = 0.f;

    RayDesc rayDesc;
    rayDesc.TMin        = u_updateProbesParams.probeRaysMinT;
    rayDesc.TMax        = u_updateProbesParams.probeRaysMaxT;
    rayDesc.Origin      = probeWorldLocation;
    rayDesc.Direction   = rayDirection;

    //TraceRay(u_worldAccelerationStructure,
    //         0,
    //         0xFF,
    //         0,
    //         1,
    //         0,
    //         rayDesc,
    //         payload);

    u_traceRaysResultTexture[dispatchIdx] = float4(payload.radiance, payload.hitDstance);
}


[shader("miss")]
void DDGIProbeRaysRTM(inout DDGIRayPayload payload)
{
    payload.radiance = float3(0.f, 0.2f, 0.7f);
    payload.hitDstance = 0.f;
}
