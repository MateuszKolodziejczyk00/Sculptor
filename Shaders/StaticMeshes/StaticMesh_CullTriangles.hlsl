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
        const bool isTriangleVisible = true;

        if(isTriangleVisible)
        {
            const uint2 trianglesVisibleBallot = WaveActiveBallot(isTriangleVisible).xy;
        
            const uint visibleTrianglesNum = countbits(trianglesVisibleBallot.x) + countbits(trianglesVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(indirectDrawCommandParams[0].vertexCount, visibleTrianglesNum * 3, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx / 3) + GetCompactedIndex(trianglesVisibleBallot, WaveGetLaneIndex());

            trianglesWorkloads[outputBufferIdx] = PackTriangleWorkload(batchElementIdx, submeshIdx, localMeshletIdx, triangleIdx);
        }
    }

    if(input.globalID.x == 0)
    {
        indirectDrawCommandParams[0].instanceCount    = 1;
        indirectDrawCommandParams[0].firstVertex      = 0;
        indirectDrawCommandParams[0].firstInstance    = 0;
    }
}
