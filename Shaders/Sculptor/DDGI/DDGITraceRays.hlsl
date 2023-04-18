
#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DDGITraceRaysDS, 0)]]

[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(StaticMeshUnifiedDataDS, 3)]]

[[descriptor_set(MaterialsDS, 4)]]

[[shader_struct(MaterialPBRData)]]

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

    TraceRay(u_sceneAccelerationStructure,
             0,
             0xFF,
             0,
             1,
             0,
             rayDesc,
             payload);

    const float dist = max(dot(rayDirection, float3(0.f, 0.f, 1.f)), 0.f);
    const float3 color = dist;
    u_traceRaysResultTexture[dispatchIdx] = float4(payload.radiance, payload.hitDstance);
}


[shader("closesthit")]
void DDGIProbeRaysStaticMeshRTCH(inout DDGIRayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        payload.radiance = 0.f;
        payload.hitDstance = -RayTCurrent();
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
        normal += u_geometryData.Load<float3>(submesh.normalsOffset + offset) * barycentricCoords[triangleIdx];
    }
    normal = mul(u_renderEntitiesData[instanceData.entityIdx].transform, float4(normal, 0.f)).xyz;
    normal = normalize(normal);

    float2 uv = 0.f;
    [unroll]
    for (uint idx = 0; idx < 3; ++idx)
    {
        const uint offset = indices[idx] * 8;
        uv += u_geometryData.Load<float2>(submesh.uvsOffset + offset) * barycentricCoords[triangleIdx];
    }
    
    const MaterialPBRData material = u_materialsData.Load<MaterialPBRData>(instanceData.materialDataOffset);

    float3 baseColor = 1.f;
    if(material.baseColorTextureIdx != IDX_NONE_32)
    {
        baseColor = u_materialsTextures[material.baseColorTextureIdx].SampleLevel(u_materialTexturesSampler, uv, 0).xyz;
        baseColor = pow(baseColor, 2.2f);
    }
    baseColor *= material.baseColorFactor;

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    
    if(material.metallicRoughnessTextureIdx != IDX_NONE_32)
    {
        float2 metallicRoughness = u_materialsTextures[material.metallicRoughnessTextureIdx].SampleLevel(u_materialTexturesSampler, uv, 0).xy;
        metallic *= metallicRoughness.x;
        roughness *= metallicRoughness.y;
    }

    payload.radiance = baseColor;
    payload.hitDstance = RayTCurrent();
}


[shader("miss")]
void DDGIProbeRaysRTM(inout DDGIRayPayload payload)
{
    payload.radiance = float3(0.f, 0.2f, 0.7f);
    payload.hitDstance = 9999.f;
}
