
#define WORKLOAD_SIZE 64

[[shader_struct(SMGPUWorkloadID)]]

#define BATCH_ELEM_IDX_MASK 0x00001fff
#define LOCAL_MESHLET_IDX_MASK 0x00001fff
#define MESHLET_TRIANGLE_IDX_MASK 0x0000003f

//////////////////////////////////////////////////////////////////////////////////////////////////
// Submeshes Workloads ===========================================================================

/**
 * workload.data1 = {[19bits - free][13bits - batch element idx]}
 * workload.data2 = {[32bits - global submesh idx]}
 */

SMGPUWorkloadID PackSubmeshWorkload(uint batchElementIdx, uint globalSubmeshIdx)
{
    SMGPUWorkloadID workload;
    workload.data1 = batchElementIdx & BATCH_ELEM_IDX_MASK;
    workload.data2 = globalSubmeshIdx;
    return workload;
}


void UnpackSubmeshWorkload(SMGPUWorkloadID workload, out uint batchElementIdx, out uint globalSubmeshIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    globalSubmeshIdx = workload.data2;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Meshlets Workloads ============================================================================

/**
 * workload.data1 = {[6bits - free][13bits - local meshlet idx][13bits - batch element idx]}
 * workload.data2 = {[32bits - global submesh idx]}
 */

SMGPUWorkloadID PackMeshletWorkload(uint batchElementIdx, uint globalSubmeshIdx, uint localMeshletIdx)
{
    SMGPUWorkloadID workload;
    workload.data1 = (batchElementIdx & BATCH_ELEM_IDX_MASK) +
                    ((localMeshletIdx & LOCAL_MESHLET_IDX_MASK) << 13);
    
    workload.data2 = globalSubmeshIdx;
    return workload;
}


void UnpackMeshletWorkload(SMGPUWorkloadID workload, out uint batchElementIdx, out uint globalSubmeshIdx, out uint localMeshletIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    localMeshletIdx = (workload.data1 >> 13) & LOCAL_MESHLET_IDX_MASK;
    globalSubmeshIdx = workload.data2;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Triandles Workloads ===========================================================================

/**
 * workload.data1 = {[6bits - meshlet triangle idx][13bits - local meshlet idx][13bits - batch element idx]}
 * workload.data2 = {[32bits - global submesh idx]}
 */

SMGPUWorkloadID PackTriangleWorkload(uint batchElementIdx, uint globalSubmeshIdx, uint localMeshletIdx, uint meshletTriangleIdx)
{
    SMGPUWorkloadID workload;

    workload.data1 = (batchElementIdx & BATCH_ELEM_IDX_MASK) +
                    ((localMeshletIdx & LOCAL_MESHLET_IDX_MASK) << 13) +
                    ((meshletTriangleIdx & MESHLET_TRIANGLE_IDX_MASK) << 26);

    workload.data2 = globalSubmeshIdx;
    return workload;
}


void UnpackTriangleWorkload(SMGPUWorkloadID workload, out uint batchElementIdx, out uint globalSubmeshIdx, out uint localMeshletIdx, out uint meshletTriangleIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    localMeshletIdx = (workload.data1 >> 13) & LOCAL_MESHLET_IDX_MASK;
    meshletTriangleIdx = (workload.data1 >> 26) & MESHLET_TRIANGLE_IDX_MASK;
    globalSubmeshIdx = workload.data2;
}