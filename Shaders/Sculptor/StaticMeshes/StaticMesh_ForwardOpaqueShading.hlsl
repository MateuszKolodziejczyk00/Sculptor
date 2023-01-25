#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "RenderStages/ForwardOpaque.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(StaticMeshBatchDS, 4)]]
[[descriptor_set(SMIndirectRenderTrianglesDS, 5)]]


struct VS_INPUT
{
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4 clipSpace    : SV_POSITION;
    float3 color        : VERTEX_COLOR;
};


VS_OUTPUT StaticMeshVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint workloadIdx = input.index / 3;
    const uint trianglePrimIdx = input.index - workloadIdx * 3;

    uint batchElementIdx;
    uint submeshIdx;
    uint localMeshletIdx;
    uint triangleIdx;
    UnpackTriangleWorkload(u_triangleWorkloads[workloadIdx], batchElementIdx, submeshIdx, localMeshletIdx, triangleIdx);
    
    const RenderEntityGPUData entityData = u_renderEntitiesData[u_visibleInstances[batchElementIdx].entityIdx];
    const float4x4 instanceTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[submeshIdx];
    
    const uint meshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;
    const MeshletGPUData meshlet = u_meshlets[meshletIdx];

    const uint triangleStride = 3;
    const uint primitiveOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset + triangleIdx * triangleStride + trianglePrimIdx;

    const uint primitiveOffsetToLoad = primitiveOffset & 0xfffffffc;

    const uint meshletPrimitiveData = u_geometryData.Load<uint>(primitiveOffsetToLoad);

    const uint primitiveIdxMask = 0x000000ff;
    const uint primIdx = (meshletPrimitiveData >> ((primitiveOffset - primitiveOffsetToLoad) << 3)) & primitiveIdxMask;
    
    const uint verticesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;

    const uint vertexIdx = u_geometryData.Load<uint>(verticesOffset + (primIdx << 2));

    const float3 vertexLocation = u_geometryData.Load<float3>(submesh.locationsOffset + vertexIdx * 12);
    const float3 vertexNormal = u_geometryData.Load<float3>(submesh.normalsOffset + vertexIdx * 12);
    
    const float3 vertexWorldLocation = mul(instanceTransform, float4(vertexLocation, 1.f)).xyz;
    const float3 vertexWorldNormal = mul(instanceTransform, float4(vertexNormal, 0.f)).xyz;

    const uint meshletHash = HashPCG(localMeshletIdx);
    const float3 color = float3(float(meshletHash & 255), float((meshletHash >> 8) & 255), float((meshletHash >> 16) & 255)) / 255.0;

    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(vertexWorldLocation, 1.f));
    //output.vertexNormal = vertexNormal;
    output.color = color;

    return output;
}


FO_PS_OUTPUT StaticMeshFS(VS_OUTPUT vertexInput)
{
    FO_PS_OUTPUT output;

    output.radiance = float4(vertexInput.color, 1.f);
    output.normals = 0.f;
    return output;
}
