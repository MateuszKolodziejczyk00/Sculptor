
[[shader_struct(GPUWorkloadID)]]

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
