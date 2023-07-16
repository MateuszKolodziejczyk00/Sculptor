
#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DDGITraceRaysDS, 0)]]

[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(StaticMeshUnifiedDataDS, 3)]]

[[descriptor_set(MaterialsDS, 4)]]

[[descriptor_set(DDGILightsDS, 5)]]

[[descriptor_set(ShadowMapsDS, 6)]]

[[shader_struct(MaterialPBRData)]]

#include "DDGI/DDGITypes.hlsli"
#include "DDGI/DDGILighting.hlsli"


struct DDGIRayPayload
{
    half3 normal;
    half roughness;
    uint baseColorMetallic;
    float hitDistance;
};


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

    if(payload.hitDistance > 0.01f)
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

        const float3 illuminance = SampleIlluminance(u_ddgiParams, u_probesIlluminanceTexture, u_probesDataSampler, u_probesHitDistanceTexture, u_probesDataSampler, worldLocation, payload.normal, -rayDirection);

        luminance += Diffuse_Lambert(illuminance) * min(surface.diffuseColor, 0.9f);
    }
    else if (payload.hitDistance >= -0.0001f)
    {
        luminance = lerp(u_updateProbesParams.groundIlluminance, u_updateProbesParams.skyIlluminance, Pow2(1.f - rayDirection.z * 0.5f + 0.5f));
    }

    u_traceRaysResultTexture[dispatchIdx] = float4(luminance, payload.hitDistance);
}


[shader("closesthit")]
void DDGIProbeRaysStaticMeshRTCH(inout DDGIRayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        payload.hitDistance = -RayTCurrent();
        return;
    }
    
    const RTInstanceData instanceData = u_rtInstances[InstanceID()];

    const SubmeshGPUData submesh = u_submeshes[instanceData.geometryDataID];
    const uint triangleIdx = PrimitiveIndex();

    const float3 barycentricCoords = float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);

    uint3 indices;
    [unroll]
    for (uint idx = 0; idx < 3; ++idx)
    {
        const uint offset = (triangleIdx * 3 + idx) * 4;
        indices[idx] = u_geometryData.Load<uint>(submesh.indicesOffset + offset);
    }

    float3 normal = 0.f;
    [unroll]
    for (uint idx = 0; idx < 3; ++idx)
    {
        const uint offset = indices[idx] * 12;
        normal += u_geometryData.Load<float3>(submesh.normalsOffset + offset) * barycentricCoords[idx];
    }
    normal = mul(u_renderEntitiesData[instanceData.entityIdx].transform, float4(normal, 0.f)).xyz;
    normal = normalize(normal);

    float2 uv = 0.f;
    [unroll]
    for (uint idx = 0; idx < 3; ++idx)
    {
        const uint offset = indices[idx] * 8;
        uv += u_geometryData.Load<float2>(submesh.uvsOffset + offset) * barycentricCoords[idx];
    }
    
    const MaterialPBRData material = u_materialsData.Load<MaterialPBRData>(instanceData.materialDataOffset);

    float3 baseColor = 1.f;
    if(material.baseColorTextureIdx != IDX_NONE_32)
    {
        baseColor = u_materialsTextures[material.baseColorTextureIdx].SampleLevel(u_materialTexturesSampler, uv, 3).xyz;
        baseColor = pow(baseColor, 2.2f);
    }
    baseColor *= material.baseColorFactor;

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    
    if(material.metallicRoughnessTextureIdx != IDX_NONE_32)
    {
        float2 metallicRoughness = u_materialsTextures[material.metallicRoughnessTextureIdx].SampleLevel(u_materialTexturesSampler, uv, 3).xy;
        metallic *= metallicRoughness.x;
        roughness *= metallicRoughness.y;
    }

    payload.normal              = half3(normal);
    payload.roughness           = half(roughness);
    payload.baseColorMetallic   = PackFloat4x8(float4(baseColor.xyz, metallic));
    payload.hitDistance         = RayTCurrent();
}


[shader("miss")]
void DDGIProbeRaysRTM(inout DDGIRayPayload payload)
{
    payload.normal              = 0.f;
    payload.roughness           = 0.f;
    payload.baseColorMetallic   = PackFloat4x8(float4(0.f, 0.5f, 1.f, 0.f));
    payload.hitDistance         = 0.f;
}


[shader("miss")]
void DDGIShadowRaysRTM(inout DDGIShadowRayPayload payload)
{
    payload.isShadowed = false;
}
