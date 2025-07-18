#include "SculptorShader.hlsli"

#include "SpecularReflections/RTGITracingDescriptors.hlsli"
#include "Utils/Packing.hlsli"

#define SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL 2

#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH


[shader("closesthit")]
void RTGI_RT_CHS(inout RTGIRayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
#ifndef SPT_MATERIAL_DOUBLE_SIDED
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        payload.distance = -RayTCurrent();
        return;
    }
#endif // SPT_MATERIAL_DOUBLE_SIDED

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

#ifdef SPT_MATERIAL_DOUBLE_SIDED
	if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
	{
		normal = -normal;
	}
#endif // SPT_MATERIAL_DOUBLE_SIDED

    float2 uv = 0.f;
    [unroll]
    for (uint idx = 0; idx < 3; ++idx)
    {
        const uint offset = indices[idx] * 8;
        uv += u_geometryData.Load<float2>(submesh.uvsOffset + offset) * barycentricCoords[idx];
    }

    const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(instanceData.materialDataID);

    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.clipSpace = -1.f;
    materialEvalParams.normal = normal;
    materialEvalParams.hasTangent = false;
    materialEvalParams.uv = uv;
    materialEvalParams.worldLocation = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

    const MaterialEvaluationOutput evaluatedMaterial = EvaluateMaterial(materialEvalParams, materialData);

    half3 emissive = half3(evaluatedMaterial.emissiveColor);

    if(any(isnan(emissive)))
    {
        emissive = 0.f;
    }

    payload.normal            = half3(evaluatedMaterial.shadingNormal);
    payload.roughness         = half(evaluatedMaterial.roughness);
    payload.baseColorMetallic = PackFloat4x8(float4(evaluatedMaterial.baseColor.xyz, evaluatedMaterial.metallic));
    payload.distance          = RayTCurrent();
    payload.emissive          = emissive;
}


[shader("anyhit")]
void RTGI_RT_AHS(inout RTGIRayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
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

    const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(instanceData.materialDataID);

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
