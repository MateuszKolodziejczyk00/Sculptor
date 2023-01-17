
#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(StaticMeshBatchDS, 1)]]

[[descriptor_set(StaticMeshSubmeshesWorkloadsDS, 2)]]

#define SUBMESH_DATA_SIZE 12

groupshared uint outputIdx;


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


/* Based on https://frostbite-wp-prd.s3.amazonaws.com/wp-content/uploads/2016/03/29204330/GDC_2016_Compute.pdf slide 38 */
uint GetCompactedIndex(uint2 valueBallot, uint laneIdx)
{
    uint2 compactMask;
    compactMask.x = laneIdx >= 32 ? ~0 : ((1U <<  laneIdx) - 1);
    compactMask.y = laneIdx  < 32 ?  0 : ((1U << (laneIdx - 32)) - 1);
    return countbits(valueBallot.x & compactMask.x)
         + countbits(valueBallot.y & compactMask.y);
}


/**
 * workload.data1 = {[16bits - free][16bits - batch element idx]}
 * workload.data2 = {[32bits - submeshID]}
 */
const uint batchElemIdxMask = 0x00ff;

GPUWorkloadID PackSubmeshWorkload(uint batchElementIdx, uint submeshID)
{
    GPUWorkloadID workload;
    workload.data1 = batchElementIdx & batchElemIdxMask;
    workload.data2 = submeshID;
    return workload;
}


void UnpackSubmeshWorkload(GPUWorkloadID workload, out uint batchElementIdx, out uint submeshID)
{
    batchElementIdx = workload.data1 & batchElemIdxMask;
    submeshID = workload.data2;
}


#define WORKLOAD_SIZE 64


[numthreads(64, 1, 1)]
void CullSubmeshesCS(CS_INPUT input)
{
    // Initialize idx value
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

    // Wait for all threads to output idx
    AllMemoryBarrier();

    if(input.globalID.x == 0)
    {
        const uint submeshesNum = submeshesWorkloadsNum[0];
        indirectDispatchSubmeshesParams[0] = submeshesNum;
        indirectDispatchSubmeshesParams[1] = 1;
        indirectDispatchSubmeshesParams[2] = 1;
    }
}
