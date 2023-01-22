#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(GeometryDS, 4)]]

[[descriptor_set(SMCullTrianglesDS, 5)]]


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
    UnpackMeshletWorkload(u_meshletWorkloads[input.groupID.x], batchElementIdx, submeshIdx, localMeshletIdx);

    const SubmeshGPUData submesh = u_submeshes[submeshIdx];
    const uint meshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;

    const MeshletGPUData meshlet = u_meshlets[meshletIdx];
 
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
                InterlockedAdd(u_drawTrianglesParams[0].vertexCount, visibleTrianglesNum * 3, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx / 3) + GetCompactedIndex(trianglesVisibleBallot, WaveGetLaneIndex());

            u_triangleWorkloads[outputBufferIdx] = PackTriangleWorkload(batchElementIdx, submeshIdx, localMeshletIdx, triangleIdx);
        }
    }

    if(input.globalID.x == 0)
    {
        u_drawTrianglesParams[0].instanceCount    = 1;
        u_drawTrianglesParams[0].firstVertex      = 0;
        u_drawTrianglesParams[0].firstInstance    = 0;
    }
}
