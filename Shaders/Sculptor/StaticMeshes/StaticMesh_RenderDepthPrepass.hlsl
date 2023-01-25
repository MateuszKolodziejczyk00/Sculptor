#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(StaticMeshBatchDS, 4)]]
[[descriptor_set(SMDepthPrepassDrawInstancesDS, 5)]]


struct VS_INPUT
{
    [[vk::builtin("DrawIndex")]] uint drawIndex : DRAW_INDEX;
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4 clipSpace    : SV_POSITION;
};


struct PS_OUTPUT
{
};


VS_OUTPUT StaticMesh_DepthPrepassVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const SMDepthPrepassDrawCallData drawCall = u_drawCommands[input.drawIndex];

    const uint renderEntityIdx = u_batchElements[drawCall.batchElementIdx].entityIdx;

    const RenderEntityGPUData entityData = u_renderEntitiesData[renderEntityIdx];
    const float4x4 instanceTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[drawCall.submeshGlobalIdx];
    
    const uint vertexIdx = u_geometryData.Load<uint>(submesh.indicesOffset + input.index * 4);

    const float3 vertexLocation = u_geometryData.Load<float3>(submesh.locationsOffset + vertexIdx * 12);
    const float3 vertexWorldLocation = mul(instanceTransform, float4(vertexLocation, 1.f)).xyz;

    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(vertexWorldLocation, 1.f));
    
    return output;
}


PS_OUTPUT StaticMesh_DepthPrepassFS(VS_OUTPUT vertexInput)
{
    PS_OUTPUT output;
    return output;
}
