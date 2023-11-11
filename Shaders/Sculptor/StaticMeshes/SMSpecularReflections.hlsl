#include "SculptorShader.hlsli"

#include "SpecularReflections/SpecularReflectionsTracingCommon.hlsli"

#define SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL 3

#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH


[shader("closesthit")]
void SpecularReflections_RT_CHS(inout SpecularReflectionsRayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        payload.distance = -RayTCurrent();
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

    const SPT_MATERIAL_DATA_TYPE materialData = u_materialsData.Load<SPT_MATERIAL_DATA_TYPE>(instanceData.materialDataOffset);

    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.clipSpace = -1.f;
    materialEvalParams.normal = normal;
    materialEvalParams.hasTangent = false;
    materialEvalParams.uv = uv;
    materialEvalParams.worldLocation = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    const MaterialEvaluationOutput evaluatedMaterial = EvaluateMaterial(materialEvalParams, materialData);

    payload.normal            = half3(evaluatedMaterial.shadingNormal);
    payload.roughness         = half(evaluatedMaterial.roughness);
    payload.baseColorMetallic = PackFloat4x8(float4(evaluatedMaterial.baseColor.xyz, evaluatedMaterial.metallic));
    payload.distance          = RayTCurrent();
    payload.emissive          = half3(evaluatedMaterial.emissiveColor);
}


[shader("anyhit")]
void SpecularReflections_RT_AHS(inout SpecularReflectionsRayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
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

    float2 uv = 0.f;
    [unroll]
    for (uint idx = 0; idx < 3; ++idx)
    {
        const uint offset = indices[idx] * 8;
        uv += u_geometryData.Load<float2>(submesh.uvsOffset + offset) * barycentricCoords[idx];
    }

    const SPT_MATERIAL_DATA_TYPE materialData = u_materialsData.Load<SPT_MATERIAL_DATA_TYPE>(instanceData.materialDataOffset);

    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.clipSpace = -1.f;
    materialEvalParams.hasTangent = false;
    materialEvalParams.uv = uv;
    materialEvalParams.worldLocation = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);
    if(opacityOutput.shouldDiscard)
    {
        IgnoreHit();
    }
}
