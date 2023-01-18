#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(StaticMeshBatchDS, 1)]]

[[descriptor_set(StaticMeshSubmeshesWorkloadsDS, 2)]]
[[descriptor_set(StaticMeshMeshletsWorkloadsDS, 3)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


[numthreads(64, 1, 1)]
void CullMeshletsCS(CS_INPUT input)
{
    // Initialize num value
    if (input.globalID.x == 0)
    {
        meshletsWorkloadsNum[0] = 0;
    }

    AllMemoryBarrier();
    
    uint batchElementIdx;
    uint submeshIdx;
    UnpackSubmeshWorkload(submeshesWorkloads[input.groupID.x], batchElementIdx, submeshIdx);

    const SubmeshGPUData submesh = submeshes[submeshIdx];
    
    for (uint idx = input.localID.x; idx < submesh.meshletsNum; idx += WORKLOAD_SIZE)
    {
        const uint globalMeshletIdx = submesh.meshletsBeginIdx + idx;
        
        const bool isMeshletVisible = true;

        if(isMeshletVisible)
        {
            const uint2 meshletsVisibleBallot = WaveActiveBallot(isMeshletVisible).xy;
        
            const uint visibleMeshletsNum = countbits(meshletsVisibleBallot.x) + countbits(meshletsVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(meshletsWorkloadsNum[0], visibleMeshletsNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(meshletsVisibleBallot, WaveGetLaneIndex());

            meshletsWorkloads[outputBufferIdx] = PackMeshletWorkload(batchElementIdx, globalMeshletIdx);
        }
    }

    // Wait for all threads to write workloads
    AllMemoryBarrier();

    if(input.globalID.x == 0)
    {
        const uint meshletsNum = meshletsWorkloadsNum[0];
        indirectDispatchMeshletsParams[0] = meshletsNum;
        indirectDispatchMeshletsParams[1] = 1;
        indirectDispatchMeshletsParams[2] = 1;
    }
}
