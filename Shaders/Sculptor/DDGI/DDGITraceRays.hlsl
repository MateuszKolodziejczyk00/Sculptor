
#include "SculptorShader.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"

#include "DDGI/DDGITraceRaysCommon.hlsli"

#include "DDGI/DDGITypes.hlsli"
#include "DDGI/DDGILighting.hlsli"


[shader("raygeneration")]
void DDGIProbeRaysRTG()
{
    const uint2 dispatchIdx = DispatchRaysIndex().xy;
    
    const uint rayIdx = dispatchIdx.y;
    const uint3 probeCoords = ComputeUpdatedProbeCoords(dispatchIdx.x, u_updateProbesParams.probesToUpdateCoords, u_updateProbesParams.probesToUpdateCount);
    const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(u_ddgiParams, probeCoords);

    float3 rayDirection = GetProbeRayDirection(rayIdx, u_updateProbesParams.raysNumPerProbe);
    rayDirection = mul(u_updateProbesParams.raysRotation, float4(rayDirection, 0.f)).xyz;

    const float3 probeWorldLocation = GetProbeWorldLocation(u_ddgiParams, probeCoords);

    DDGIRayPayload payload;

    RayDesc rayDesc;
    rayDesc.TMin        = u_updateProbesParams.probeRaysMinT;
    rayDesc.TMax        = u_updateProbesParams.probeRaysMaxT;
    rayDesc.Origin      = probeWorldLocation;
    rayDesc.Direction   = rayDirection;

    TraceRay(u_sceneAccelerationStructure,
             0,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             payload);

    float3 luminance = 0.f;

    if(payload.hitDistance > 0.f)
    {
        float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

        const float3 worldLocation = probeWorldLocation + rayDirection * payload.hitDistance;
        
        ShadedSurface surface;
        surface.location        = worldLocation;
        surface.shadingNormal   = payload.normal;
        surface.geometryNormal  = payload.normal;
        surface.roughness       = payload.roughness;
        ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, surface.diffuseColor, surface.specularColor);
        
        luminance = CalcReflectedLuminance(surface, -rayDirection);

        const float3 illuminance = SampleIlluminance(u_ddgiParams, u_probesIlluminanceTexture, u_linearSampler, u_probesHitDistanceTexture, u_linearSampler, worldLocation, payload.normal, -rayDirection);

        luminance += Diffuse_Lambert(illuminance) * min(surface.diffuseColor, 0.9f);
    }
    else if (payload.hitDistance > u_updateProbesParams.probeRaysMaxT)
    {
        luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, probeWorldLocation, rayDirection);
        payload.hitDistance = u_updateProbesParams.probeRaysMaxT;
    }

    u_traceRaysResultTexture[dispatchIdx] = float4(luminance, payload.hitDistance);
}


[shader("miss")]
void DDGIProbeRaysRTM(inout DDGIRayPayload payload)
{
    payload.normal              = 0.f;
    payload.roughness           = 0.f;
    payload.baseColorMetallic   = PackFloat4x8(float4(0.f, 0.5f, 1.f, 0.f));
    payload.hitDistance         = u_updateProbesParams.probeRaysMaxT + 1000.f;;
}


[shader("miss")]
void DDGIShadowRaysRTM(inout DDGIShadowRayPayload payload)
{
    payload.isShadowed = false;
}
