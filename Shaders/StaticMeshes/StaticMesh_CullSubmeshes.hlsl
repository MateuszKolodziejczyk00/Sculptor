#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(StaticMeshBatchDS, 1)]]

[[descriptor_set(StaticMeshSubmeshesWorkloadsDS, 2)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


[numthreads(64, 1, 1)]
void CullSubmeshesCS(CS_INPUT input)
{
    // Initialize num value
    if (input.globalID.x == 0)
    {
        submeshesWorkloadsNum[0] = 0;
    }

    AllMemoryBarrier();
    
    const uint batchElementIdx = input.groupID.x;

    const uint meshIdx = batchElements[batchElementIdx].staticMeshIdx;
    
    const StaticMeshGPUData staticMesh = staticMeshes[meshIdx];
    
    for (uint idx = input.localID.x; idx < staticMesh.submeshesNum; idx += WORKLOAD_SIZE)
    {
        const uint globalSubmeshIdx = staticMesh.submeshesBeginIdx + idx;
        
        const bool isSubmeshVisible = true;

        if(isSubmeshVisible)
        {
            const uint2 submeshVisibleBallot = WaveActiveBallot(isSubmeshVisible).xy;
        
            const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(submeshesWorkloadsNum[0], visibleSubmeshesNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

            const GPUWorkloadID submeshWorkload = PackSubmeshWorkload(batchElementIdx, globalSubmeshIdx);
            submeshesWorkloads[outputBufferIdx] = submeshWorkload;
        }
    }

    // Wait for all threads to wrie submeshes
    AllMemoryBarrier();

    if(input.globalID.x == 0)
    {
        const uint submeshesNum = submeshesWorkloadsNum[0];
        indirectDispatchSubmeshesParams[0] = submeshesNum;
        indirectDispatchSubmeshesParams[1] = 1;
        indirectDispatchSubmeshesParams[2] = 1;
    }
}
