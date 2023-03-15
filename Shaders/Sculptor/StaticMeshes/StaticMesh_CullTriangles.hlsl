#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(RenderViewDS, 3)]]

[[descriptor_set(GeometryDS, 4)]]

[[descriptor_set(SMCullTrianglesDS, 5)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


uint3 LoadTriangleVertexIndices(uint meshletPrimitivesOffset, uint verticesOffset, uint meshletTriangleIdx)
{
    const uint triangleStride = 3;
    const uint primitiveOffset = meshletPrimitivesOffset + meshletTriangleIdx * triangleStride;

    // Load multiple of 4
    const uint primitiveOffsetToLoad = primitiveOffset & 0xfffffffc;

    // Load uint2 to be sure that we will have all 3 indices
    const uint2 meshletPrimitivesIndices = u_geometryData.Load<uint2>(primitiveOffsetToLoad);

    uint primitiveIndicesByteOffset = primitiveOffset - primitiveOffsetToLoad;

    uint3 traingleIndices;

    for (int idx = 0; idx < 3; ++idx, ++primitiveIndicesByteOffset)
    {
        uint indices4;
        uint offset;
        
        if(primitiveIndicesByteOffset >= 4)
        {
            indices4 = meshletPrimitivesIndices[1];
            offset = primitiveIndicesByteOffset - 4;
        }
        else
        {
            indices4 = meshletPrimitivesIndices[0];
            offset = primitiveIndicesByteOffset;
        }

        traingleIndices[idx] = (indices4 >> (offset * 8)) & 0x000000ff;
    }

    for (int idx = 0; idx < 3; ++idx, ++primitiveIndicesByteOffset)
    {
        traingleIndices[idx] = u_geometryData.Load<uint>(verticesOffset + (traingleIndices[idx] * 4));
    }
    
    return traingleIndices;
}


[numthreads(64, 1, 1)]
void CullTrianglesCS(CS_INPUT input)
{
    uint batchElementIdx;
    uint localMeshletIdx;
    UnpackMeshletWorkload(u_meshletWorkloads[input.groupID.x], batchElementIdx, localMeshletIdx);

    uint submeshIdx = u_visibleBatchElements[batchElementIdx].submeshGlobalIdx;

    const uint entityIdx = u_visibleBatchElements[batchElementIdx].entityIdx;
    const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];
    const float4x4 entityTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[submeshIdx];
    const uint meshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;

    const MeshletGPUData meshlet = u_meshlets[meshletIdx];
 
    const uint triangleIdx = input.localID.x;

    if(triangleIdx < meshlet.triangleCount)
    {
        const uint primitiveIndicesOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset;
        const uint verticesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;
        const uint locationsOffset = submesh.locationsOffset;

        bool isTriangleVisible = true;

        const float nearPlane = GetNearPlane(u_sceneView.projectionMatrix);

        const uint3 triangleVertexIndices = LoadTriangleVertexIndices(primitiveIndicesOffset, verticesOffset, triangleIdx);
        
        bool isInFrontOfPerspectivePlane = true;

        float3 triangleVerticesClip[3];
        for (int idx = 0; idx < 3; ++idx)
        {
            const float3 vertexLocation = u_geometryData.Load<float3>(locationsOffset + triangleVertexIndices[idx] * 12);
            const float4 vertexView = mul(u_sceneView.viewMatrix, mul(entityTransform, float4(vertexLocation, 1.f)));
            const float4 vertexClip = mul(u_sceneView.projectionMatrix, vertexView);
            isInFrontOfPerspectivePlane = isInFrontOfPerspectivePlane && vertexView.x > nearPlane;
            triangleVerticesClip[idx] = vertexClip.xyw;
        }

        // currently we're not handling case when some vertices are in front of near plane, so we just pass those triangles as visible
        if(isInFrontOfPerspectivePlane)
        {
            // Perspective division
            for (int idx = 0; idx < 3; ++idx)
            {
                triangleVerticesClip[idx] = triangleVerticesClip[idx] / triangleVerticesClip[idx].z;
            }

            const float2 ca = triangleVerticesClip[1] - triangleVerticesClip[0];
            const float2 cb = triangleVerticesClip[2] - triangleVerticesClip[0];

            // bacface culling
            isTriangleVisible = isTriangleVisible && (ca.x * cb.y >= ca.y * cb.x);
        }
        
        if(isTriangleVisible && isInFrontOfPerspectivePlane)
        {
            float4 aabb;
            // Transform [-1, 1] to [0, resolution]
            aabb.xy = (min(triangleVerticesClip[0].xy, min(triangleVerticesClip[1].xy, triangleVerticesClip[2].xy)) + 1.f) * 0.5f * u_viewRenderingParams.renderingResolution;
            aabb.zw = (max(triangleVerticesClip[0].xy, max(triangleVerticesClip[1].xy, triangleVerticesClip[2].xy)) + 1.f) * 0.5f * u_viewRenderingParams.renderingResolution;

            // This is based on Niagara renderer created by Arseny Kapoulkine
            // Source: https://github.com/zeux/niagara/blob/master/src/shaders/meshlet.mesh.glsl
            // Original comment: "this can be set to 1/2^subpixelPrecisionBits"
            const float subpixelPrecision = 1.f / 256.f;

            // small primitive culling
            // Original comment: "this is slightly imprecise (doesn't fully match hw behavior and is both too loose and too strict)"
            isTriangleVisible = isTriangleVisible && (round(aabb.x - subpixelPrecision) != round(aabb.z) && round(aabb.y) != round(aabb.w + subpixelPrecision));
        }

        if(isTriangleVisible)
        {
            const uint2 trianglesVisibleBallot = WaveActiveBallot(isTriangleVisible).xy;
        
            const uint visibleTrianglesNum = countbits(trianglesVisibleBallot.x) + countbits(trianglesVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(u_drawTrianglesParams[0].vertexCount, visibleTrianglesNum * 3, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx / 3) + GetCompactedIndex(trianglesVisibleBallot, WaveGetLaneIndex());

            u_triangleWorkloads[outputBufferIdx] = PackTriangleWorkload(batchElementIdx, localMeshletIdx, triangleIdx);
        }
    }

    if(input.globalID.x == 0)
    {
        u_drawTrianglesParams[0].instanceCount    = 1;
        u_drawTrianglesParams[0].firstVertex      = 0;
        u_drawTrianglesParams[0].firstInstance    = 0;
    }
}
