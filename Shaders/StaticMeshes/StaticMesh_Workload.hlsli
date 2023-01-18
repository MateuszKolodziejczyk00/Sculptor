
#define WORKLOAD_SIZE 64

[[shader_struct(GPUWorkloadID)]]

#define BATCH_ELEM_IDX_MASK 0x00ff;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Submeshes Workloads ===========================================================================

/**
 * workload.data1 = {[16bits - free][16bits - batch element idx]}
 * workload.data2 = {[32bits - submeshIdx]}
 */

GPUWorkloadID PackSubmeshWorkload(uint batchElementIdx, uint submeshIdx)
{
    GPUWorkloadID workload;
    workload.data1 = batchElementIdx & BATCH_ELEM_IDX_MASK;
    workload.data2 = submeshIdx;
    return workload;
}


void UnpackSubmeshWorkload(GPUWorkloadID workload, out uint batchElementIdx, out uint submeshIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    submeshIdx = workload.data2;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Meshlets Workloads ============================================================================

/**
 * workload.data1 = {[16bits - free][16bits - batch element idx]}
 * workload.data2 = {[32bits - meshletIdx]}
 */

GPUWorkloadID PackMeshletWorkload(uint batchElementIdx, uint meshletIdx)
{
    GPUWorkloadID workload;
    workload.data1 = batchElementIdx & BATCH_ELEM_IDX_MASK;
    workload.data2 = meshletIdx;
    return workload;
}


void UnpackMeshletWorkload(GPUWorkloadID workload, out uint batchElementIdx, out uint meshletIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    meshletIdx = workload.data2;
}

