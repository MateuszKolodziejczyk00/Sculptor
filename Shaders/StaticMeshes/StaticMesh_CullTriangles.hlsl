#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(StaticMeshBatchDS, 1)]]

[[descriptor_set(GeometryDS, 2)]]

[[descriptor_set(StaticMeshMeshletsWorkloadsDS, 3)]]
[[descriptor_set(StaticMeshTrianglesWorkloadsDS, 4)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


[numthreads(64, 1, 1)]
void CullTrianglesCS(CS_INPUT input)
{
    uint batchElementIdx;
    uint submeshIdx;
    uint localMeshletIdx;
    UnpackMeshletWorkload(meshletsWorkloads[input.groupID.x], batchElementIdx, submeshIdx, localMeshletIdx);

    const SubmeshGPUData submesh = submeshes[submeshIdx];
    const uint meshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;

    const MeshletGPUData meshlet = meshlets[meshletIdx];
 
    const uint triangleIdx = input.localID.x;

    if(triangleIdx < meshlet.triangleCount)
    {
        const uint triangleStride = 4;
        const uint primitivesOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset + triangleIdx * triangleStride;

        const uint meshletPrimitiveData = geometryData.Load<uint>(primitivesOffset);

        const uint primitiveIdxMask = 0x000000ff;
        const uint prim0 = meshletPrimitiveData & primitiveIdxMask;
        const uint prim1 = (meshletPrimitiveData >> 8) & primitiveIdxMask;
        const uint prim2 = (meshletPrimitiveData >> 16) & primitiveIdxMask;
        
        const uint verticesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;

        const uint vertex0 = geometryData.Load<uint>(verticesOffset + (prim0 << 2));
        const uint vertex1 = geometryData.Load<uint>(verticesOffset + (prim1 << 2));
        const uint vertex2 = geometryData.Load<uint>(verticesOffset + (prim2 << 2));

        const uint locationsOffset = submesh.locationsOffset;

        const float3 location0 = geometryData.Load<float3>(locationsOffset + vertex0 * 12);
        const float3 location1 = geometryData.Load<float3>(locationsOffset + vertex1 * 12);
        const float3 location2 = geometryData.Load<float3>(locationsOffset + vertex2 * 12);
        
        const bool isTriangleVisible = true;

        if(isTriangleVisible)
        {
            const uint2 trianglesVisibleBallot = WaveActiveBallot(isTriangleVisible).xy;
        
            const uint visibleTrianglesNum = countbits(trianglesVisibleBallot.x) + countbits(trianglesVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(indirectDispatchTrianglesParams[0], visibleTrianglesNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(trianglesVisibleBallot, WaveGetLaneIndex());

            trianglesWorkloads[outputBufferIdx] = PackTriangleWorkload(batchElementIdx, submeshIdx, localMeshletIdx, triangleIdx);
        }
    }

    if(input.globalID.x == 0)
    {
        indirectDispatchTrianglesParams[1] = 1;
        indirectDispatchTrianglesParams[2] = 1;
    }
}
