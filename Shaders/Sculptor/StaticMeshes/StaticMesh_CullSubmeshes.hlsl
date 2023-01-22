#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(SMCullSubmeshesDS, 4)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


[numthreads(64, 1, 1)]
void CullSubmeshesCS(CS_INPUT input)
{
    const uint batchElementIdx = uint(u_visibleInstancesIndices[input.groupID.x]);

    const uint meshIdx = u_batchElements[batchElementIdx].staticMeshIdx;
    
    const StaticMeshGPUData staticMesh = u_staticMeshes[meshIdx];
    
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
                InterlockedAdd(u_dispatchSubmeshesParams[0], visibleSubmeshesNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

            const GPUWorkloadID submeshWorkload = PackSubmeshWorkload(batchElementIdx, globalSubmeshIdx);
            u_submeshWorkloads[outputBufferIdx] = submeshWorkload;
        }
    }

    if(input.globalID.x == 0)
    {
        u_dispatchSubmeshesParams[1] = 1;
        u_dispatchSubmeshesParams[2] = 1;
    }
}
