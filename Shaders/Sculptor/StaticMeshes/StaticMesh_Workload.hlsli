
#define WORKLOAD_SIZE 64

[[shader_struct(SMGPUWorkloadID)]]

#define BATCH_ELEM_IDX_MASK 0x00001fff
#define LOCAL_MESHLET_IDX_MASK 0x00001fff
#define MESHLET_TRIANGLE_IDX_MASK 0x0000003f

//////////////////////////////////////////////////////////////////////////////////////////////////
// Meshlets Workloads ============================================================================

/**
 * workload.data1 = {[6bits - free][13bits - local meshlet idx][13bits - batch element idx]}
 */

SMGPUWorkloadID PackMeshletWorkload(uint batchElementIdx, uint localMeshletIdx)
{
    SMGPUWorkloadID workload;
    workload.data1 = (batchElementIdx & BATCH_ELEM_IDX_MASK) +
                    ((localMeshletIdx & LOCAL_MESHLET_IDX_MASK) << 13);
    
    return workload;
}


void UnpackMeshletWorkload(SMGPUWorkloadID workload, out uint batchElementIdx, out uint localMeshletIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    localMeshletIdx = (workload.data1 >> 13) & LOCAL_MESHLET_IDX_MASK;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Triandles Workloads ===========================================================================

/**
 * workload.data1 = {[6bits - meshlet triangle idx][13bits - local meshlet idx][13bits - batch element idx]}
 */

SMGPUWorkloadID PackTriangleWorkload(uint batchElementIdx, uint localMeshletIdx, uint meshletTriangleIdx)
{
    SMGPUWorkloadID workload;

    workload.data1 = (batchElementIdx & BATCH_ELEM_IDX_MASK) +
                    ((localMeshletIdx & LOCAL_MESHLET_IDX_MASK) << 13) +
                    ((meshletTriangleIdx & MESHLET_TRIANGLE_IDX_MASK) << 26);

    return workload;
}


void UnpackTriangleWorkload(SMGPUWorkloadID workload, out uint batchElementIdx, out uint localMeshletIdx, out uint meshletTriangleIdx)
{
    batchElementIdx = workload.data1 & BATCH_ELEM_IDX_MASK;
    localMeshletIdx = (workload.data1 >> 13) & LOCAL_MESHLET_IDX_MASK;
    meshletTriangleIdx = (workload.data1 >> 26) & MESHLET_TRIANGLE_IDX_MASK;
}
