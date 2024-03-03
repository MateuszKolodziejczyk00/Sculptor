#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(MaterialsDS, 3)]]

[[descriptor_set(RenderViewDS, 4)]]

[[descriptor_set(StaticMeshBatchDS, 5)]]
[[descriptor_set(SMDepthOnlyDrawInstancesDS, 6)]]

#include "RenderStages/DepthPrepass/DepthPrepass.hlsli"


#ifdef SPT_MATERIAL_CUSTOM_OPACITY
#define FRAGMENT_SHADER_NEEDS_MATERIAL 1
#else
#define FRAGMENT_SHADER_NEEDS_MATERIAL 0
#endif // SPT_MATERIAL_CUSTOM_OPACITY


#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH


struct VS_INPUT
{
    [[vk::builtin("DrawIndex")]] uint drawIndex : DRAW_INDEX;
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4 clipSpace                : SV_POSITION;
#if FRAGMENT_SHADER_NEEDS_MATERIAL
    float2 uv                       : UV;
    uint materialDataOffset         : MATERIAL_DATA_OFFSET;
#endif // FRAGMENT_SHADER_NEEDS_MATERIAL
};


VS_OUTPUT SMDepthOnly_VS(VS_INPUT input)
{
    VS_OUTPUT output;

    const SMDepthOnlyDrawCallData drawCall = u_drawCommands[input.drawIndex];

    const uint renderEntityIdx = u_batchElements[drawCall.batchElementIdx].entityIdx;
    const uint submeshGlobalIdx = u_batchElements[drawCall.batchElementIdx].submeshGlobalIdx;

    const RenderEntityGPUData entityData = u_renderEntitiesData[renderEntityIdx];
    const float4x4 instanceTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[submeshGlobalIdx];
    
    const uint vertexIdx = u_geometryData.Load<uint>(submesh.indicesOffset + input.index * 4);

    const float3 vertexLocation = u_geometryData.Load<float3>(submesh.locationsOffset + vertexIdx * 12);
    const float3 vertexWorldLocation = mul(instanceTransform, float4(vertexLocation, 1.f)).xyz;

#if FRAGMENT_SHADER_NEEDS_MATERIAL
    output.materialDataOffset = u_batchElements[drawCall.batchElementIdx].materialDataOffset;

    const float2 vertexUV = u_geometryData.Load<float2>(submesh.uvsOffset + vertexIdx * 8);
    output.uv = vertexUV;
#endif // FRAGMENT_SHADER_NEEDS_MATERIAL

#if defined(FORCE_NO_JITTER)
    output.clipSpace = mul(u_sceneView.viewProjectionMatrixNoJitter, float4(vertexWorldLocation, 1.f));
#else
    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(vertexWorldLocation, 1.f));
#endif // FORCE_NO_JITTER
    
    return output;
}


DP_PS_OUTPUT SMDepthOnly_FS(VS_OUTPUT vertexInput)
{
    DP_PS_OUTPUT output;

#ifdef SPT_MATERIAL_CUSTOM_OPACITY
    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.uv = vertexInput.uv;
    
    const SPT_MATERIAL_DATA_TYPE materialData = u_materialsData.Load<SPT_MATERIAL_DATA_TYPE>(vertexInput.materialDataOffset);

    const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);
    if(opacityOutput.shouldDiscard)
    {
        discard;
    }
#endif // SPT_MATERIAL_CUSTOM_OPACITY

    return output;
}
