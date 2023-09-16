#include "SculptorShader.hlsli"

#include "Utils/RTVisibilityCommon.hlsli"

#define SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL 0

#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH


[shader("anyhit")]
void RTVisibility_RT_AHS(inout RTVisibilityPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
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
