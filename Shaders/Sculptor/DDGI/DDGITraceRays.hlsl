#include "SculptorShader.hlsli"

#include "DDGI/DDGITraceRaysCommon.hlsli"

#include "Atmosphere/Atmosphere.hlsli"
#include "DDGI/DDGITypes.hlsli"
#include "Lights/Lighting.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


[shader("raygeneration")]
void DDGIProbeRaysRTG()
{
    const uint2 dispatchIdx = DispatchRaysIndex().xy;
    
    const uint rayIdx = dispatchIdx.y;
    const uint3 probeCoords = ComputeUpdatedProbeCoords(dispatchIdx.x, u_relitParams.probesToUpdateCoords, u_relitParams.probesToUpdateCount);

    float3 rayDirection = GetProbeRayDirection(rayIdx, u_relitParams.raysNumPerProbe);

    const float3 probeWorldLocation = GetProbeWorldLocation(u_volumeParams, probeCoords);

    DDGIRayPayload payload;

    RayDesc rayDesc;
    rayDesc.TMin        = u_relitParams.probeRaysMinT;
    rayDesc.TMax        = u_relitParams.probeRaysMaxT;
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

    const bool isValidHit = payload.hitDistance > 0.f && payload.hitDistance <= u_relitParams.probeRaysMaxT;

    if(isValidHit)
    {
        float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

        const float3 hitNormal = payload.normal;

        // Introduce additional bias as using just distance can have too low precision (which results in artifacts f.e. when using shadow maps)
        const float locationBias = 0.03f;
        const float3 worldLocation = probeWorldLocation + rayDirection * payload.hitDistance + hitNormal * locationBias;

        ShadedSurface surface;
        surface.location        = worldLocation;
        surface.shadingNormal   = hitNormal;
        surface.geometryNormal  = hitNormal;
        surface.roughness       = payload.roughness;
        ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, surface.diffuseColor, surface.specularColor);

		const float recursionMultiplier = 0.9f;
        
        luminance = CalcReflectedLuminance(surface, -rayDirection, DDGISampleContext::Create(), recursionMultiplier);

        luminance += payload.emissive;
    }
    else if (payload.hitDistance > u_relitParams.probeRaysMaxT)
    {
        const float3 probeAtmosphereLocation = GetLocationInAtmosphere(u_atmosphereParams, probeWorldLocation);
        luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, probeAtmosphereLocation, rayDirection);
    }

    u_traceRaysResultTexture[dispatchIdx] = float4(luminance, payload.hitDistance);
}


[shader("miss")]
void DDGIProbeRaysRTM(inout DDGIRayPayload payload)
{
    payload.normal              = 0.f;
    payload.roughness           = 0.f;
    payload.baseColorMetallic   = PackFloat4x8(float4(0.f, 0.5f, 1.f, 0.f));
    payload.hitDistance         = u_relitParams.probeRaysMaxT + 1000.f;
}


[shader("miss")]
void DDGIShadowRaysRTM(inout ShadowRayPayload payload)
{
    payload.isShadowed = false;
}
