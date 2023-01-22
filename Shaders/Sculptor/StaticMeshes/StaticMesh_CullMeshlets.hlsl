#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(SMCullMeshletsDS, 4)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


[numthreads(64, 1, 1)]
void CullMeshletsCS(CS_INPUT input)
{
    uint batchElementIdx;
    uint submeshIdx;
    UnpackSubmeshWorkload(u_submeshWorkloads[input.groupID.x], batchElementIdx, submeshIdx);

    const SubmeshGPUData submesh = u_submeshes[submeshIdx];
    
    for (uint localMeshletIdx = input.localID.x; localMeshletIdx < submesh.meshletsNum; localMeshletIdx += WORKLOAD_SIZE)
    {
        const uint globalMeshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;
        
        const bool isMeshletVisible = true;

        if(isMeshletVisible)
        {
            const uint2 meshletsVisibleBallot = WaveActiveBallot(isMeshletVisible).xy;
        
            const uint visibleMeshletsNum = countbits(meshletsVisibleBallot.x) + countbits(meshletsVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(u_dispatchMeshletsParams[0], visibleMeshletsNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(meshletsVisibleBallot, WaveGetLaneIndex());

            u_meshletWorkloads[outputBufferIdx] = PackMeshletWorkload(batchElementIdx, submeshIdx, localMeshletIdx);
        }
    }

    if(input.globalID.x == 0)
    {
        u_dispatchMeshletsParams[1] = 1;
        u_dispatchMeshletsParams[2] = 1;
    }
}
