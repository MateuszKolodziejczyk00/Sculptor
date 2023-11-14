
#include "SculptorShader.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"

#include "DDGI/DDGITraceRaysCommon.hlsli"

#include "DDGI/DDGITypes.hlsli"
#include "Lights/Lighting.hlsli"


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

    TraceRay(u_sceneTLAS,
             0,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             payload);

    float3 luminance = 0.f;

    const bool isValidHit = payload.hitDistance > 0.f && payload.hitDistance <= u_updateProbesParams.probeRaysMaxT;

    if(isValidHit)
    {
        float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

        const float3 hitNormal = payload.normal;

        // Introduce additional bias as using just distance can have too low precision (which results in artifacts f.e. when using shadow maps)
        const float locationBias = 0.03f;
        const float3 worldLocation = probeWorldLocation + rayDirection * payload.hitDistance + hitNormal * locationBias;

        const float ddgiLightingMinRoughness = 0.3f;
        
        ShadedSurface surface;
        surface.location        = worldLocation;
        surface.shadingNormal   = hitNormal;
        surface.geometryNormal  = hitNormal;
        surface.roughness       = max(ddgiLightingMinRoughness, payload.roughness);
        ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, surface.diffuseColor, surface.specularColor);
        
        luminance = CalcReflectedLuminance(surface, -rayDirection);

        const float3 illuminance = DDGISampleIlluminance(u_ddgiParams, u_probesIlluminanceTexture, u_linearSampler, u_probesHitDistanceTexture, u_linearSampler, worldLocation, payload.normal, -rayDirection, payload.normal);

        luminance += payload.emissive;

        luminance += Diffuse_Lambert(illuminance) * min(surface.diffuseColor, 0.9f);
    }
    else if (payload.hitDistance > u_updateProbesParams.probeRaysMaxT)
    {
        const float3 probeAtmosphereLocation = GetLocationInAtmosphere(u_atmosphereParams, probeWorldLocation);
        luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, probeAtmosphereLocation, rayDirection);
    }

#if DDGI_DEBUG_RAYS
    const uint debugRayIdx = dispatchIdx.x * u_updateProbesParams.raysNumPerProbe + rayIdx;

    DDGIDebugRay debugRay;
    debugRay.traceOrigin        = probeWorldLocation;
    debugRay.isValidHit         = isValidHit;
    debugRay.traceEnd           = probeWorldLocation + rayDirection * abs(payload.hitDistance);
    debugRay.luminance          = luminance;
    u_debugRays[debugRayIdx]    = debugRay;
#endif // DDGI_DEBUG_RAYS

    u_traceRaysResultTexture[dispatchIdx] = float4(luminance, payload.hitDistance);
}


[shader("miss")]
void DDGIProbeRaysRTM(inout DDGIRayPayload payload)
{
    payload.normal              = 0.f;
    payload.roughness           = 0.f;
    payload.baseColorMetallic   = PackFloat4x8(float4(0.f, 0.5f, 1.f, 0.f));
    payload.hitDistance         = u_updateProbesParams.probeRaysMaxT + 1000.f;
}


[shader("miss")]
void DDGIShadowRaysRTM(inout ShadowRayPayload payload)
{
    payload.isShadowed = false;
}
