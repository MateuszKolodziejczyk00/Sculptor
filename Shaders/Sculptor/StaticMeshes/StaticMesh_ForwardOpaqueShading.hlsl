#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "RenderStages/ForwardOpaque.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(StaticMeshBatchDS, 4)]]
[[descriptor_set(SMIndirectRenderTrianglesDS, 5)]]

[[descriptor_set(MaterialsDS, 6)]]

[[shader_struct(MaterialPBRData)]]


struct VS_INPUT
{
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace           : SV_POSITION;
    uint    materialDataOffset  : MATERIAL_DATA;
    float3  normal              : VERTEX_NORMAL;
    float3  tangent             : VERTEX_TANGENT;
    bool    hasTangent          : VERTEX_HAS_TANGENT;
    float3  bitangent           : VERTEX_BITANGENT;
    float2  uv                  : VERTEX_UV;
};


VS_OUTPUT StaticMeshVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint workloadIdx = input.index / 3;
    const uint triangleVertexIdx = input.index - workloadIdx * 3;

    uint batchElementIdx;
    uint localMeshletIdx;
    uint triangleIdx;
    UnpackTriangleWorkload(u_triangleWorkloads[workloadIdx], batchElementIdx, localMeshletIdx, triangleIdx);
    
    const RenderEntityGPUData entityData = u_renderEntitiesData[u_visibleBatchElements[batchElementIdx].entityIdx];
    const float4x4 instanceTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[u_visibleBatchElements[batchElementIdx].submeshGlobalIdx];
    
    const uint meshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;
    const MeshletGPUData meshlet = u_meshlets[meshletIdx];

    const uint triangleStride = 3;
    const uint primitiveOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset + triangleIdx * triangleStride + triangleVertexIdx;

    // Load multiple of 4
    const uint primitiveOffsetToLoad = primitiveOffset & 0xfffffffc;

    const uint meshletPrimitiveData = u_geometryData.Load<uint>(primitiveOffsetToLoad);

    // Load proper byte
    const uint primitiveIdxMask = 0x000000ff;
    const uint primIdx = (meshletPrimitiveData >> ((primitiveOffset - primitiveOffsetToLoad) << 3)) & primitiveIdxMask;
    
    const uint verticesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;

    const uint vertexIdx = u_geometryData.Load<uint>(verticesOffset + (primIdx * 4));

    const float3 vertexLocation = u_geometryData.Load<float3>(submesh.locationsOffset + vertexIdx * 12);
    const float3 vertexWorldLocation = mul(instanceTransform, float4(vertexLocation, 1.f)).xyz;
    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(vertexWorldLocation, 1.f));

    output.hasTangent = false;
    if(submesh.normalsOffset != IDX_NONE_32)
    {
        const float3 vertexNormal = u_geometryData.Load<float3>(submesh.normalsOffset + vertexIdx * 12);
        output.normal = normalize(mul(instanceTransform, float4(vertexNormal, 0.f)).xyz);
        
        if(submesh.tangentsOffset != IDX_NONE_32)
        {
            const float4 vertexTangent = u_geometryData.Load<float4>(submesh.tangentsOffset + vertexIdx * 16);
            output.tangent = normalize(mul(instanceTransform, float4(vertexTangent.xyz, 0.f)).xyz);
            output.bitangent = cross(output.normal, output.tangent) * vertexTangent.w;
            output.hasTangent = true;
        }
    }
    
    if(submesh.uvsOffset != IDX_NONE_32)
    {
        output.uv = u_geometryData.Load<float2>(submesh.uvsOffset + vertexIdx * 8);
    }

    output.materialDataOffset = u_visibleBatchElements[batchElementIdx].materialDataOffset;

    return output;
}


FO_PS_OUTPUT StaticMeshFS(VS_OUTPUT vertexInput)
{
    FO_PS_OUTPUT output;

    //const uint meshletHash = HashPCG(localMeshletIdx);
    //const float3 color = float3(float(meshletHash & 255), float((meshletHash >> 8) & 255), float((meshletHash >> 16) & 255)) / 255.0;
    
    float3 color = 1.f;
    
    const MaterialPBRData material = u_materialsData.Load<MaterialPBRData>(vertexInput.materialDataOffset);
    if(material.baseColorTextureIdx != IDX_NONE_32)
    {
        color = u_materialsTextures[material.baseColorTextureIdx].Sample(u_materialTexturesSampler, vertexInput.uv).xyz;
        color = pow(color, 2.2f);
    }

    color *= material.baseColorFactor;

    output.radiance = float4(color, 1.f);

    float3 normal;
    if(vertexInput.hasTangent && material.normalsTextureIdx != IDX_NONE_32)
    {
        float3 textureNormal = u_materialsTextures[material.normalsTextureIdx].Sample(u_materialTexturesSampler, vertexInput.uv).xyz;
        textureNormal = textureNormal * 2.f - 1.f;
        const float3x3 TBN = transpose(float3x3(normalize(vertexInput.tangent), normalize(vertexInput.bitangent), normalize(vertexInput.normal)));
        normal = normalize(mul(TBN, textureNormal));
    }
    else
    {
        normal = normalize(vertexInput.normal);
    }

    output.normal = float4(normal * 0.5f + 0.5f, 1.f);
    
    return output;
}
