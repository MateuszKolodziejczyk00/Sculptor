#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[descriptor_set(StaticMeshBatchDS)]]
[[descriptor_set(SMDepthOnlyDrawInstancesDS)]]

#include "RenderStages/DepthPrepass/DepthPrepass.hlsli"
#include "SceneRendering/GPUScene.hlsli"


#if CUSTOM_OPACITY
#define FRAGMENT_SHADER_NEEDS_MATERIAL 1
#include "Materials/MaterialSystem.hlsli"
#else
#define FRAGMENT_SHADER_NEEDS_MATERIAL 0
#endif // CUSTOM_OPACITY




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
    uint   materialDataID           : MATERIAL_DATA_ID;
#endif // FRAGMENT_SHADER_NEEDS_MATERIAL
};


VS_OUTPUT SMDepthOnly_VS(VS_INPUT input)
{
    VS_OUTPUT output;

    const SMDepthOnlyDrawCallData drawCall = u_drawCommands[input.drawIndex];

	const StaticMeshBatchElement batchElem = u_batchElements[drawCall.batchElementIdx];

	const RenderEntityGPUData entityData = batchElem.entityPtr.Load();
    const float4x4 instanceTransform = entityData.transform;

	const SubmeshGPUData submesh = batchElem.submeshPtr.Load();
    
	const uint vertexIdx = UGB().LoadVertexIndex(submesh.indicesOffset, input.index);

	const float3 vertexLocation = UGB().LoadLocation(submesh.locationsOffset, vertexIdx);
    const float3 vertexWorldLocation = mul(instanceTransform, float4(vertexLocation, 1.f)).xyz;

#if FRAGMENT_SHADER_NEEDS_MATERIAL
    output.materialDataID = uint(u_batchElements[drawCall.batchElementIdx].materialDataHandle.id);

    const float2 normalizedUV = UGB().LoadNormalizedUV(submesh.uvsOffset, vertexIdx);
    const float2 vertexUV = submesh.uvsMin + normalizedUV * submesh.uvsRange;
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

#if CUSTOM_OPACITY
    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.uv = vertexInput.uv;
    
    const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(MaterialDataHandle(vertexInput.materialDataID));

    const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);
    if(opacityOutput.shouldDiscard)
    {
        discard;
    }
#endif // CUSTOM_OPACITY

    return output;
}
