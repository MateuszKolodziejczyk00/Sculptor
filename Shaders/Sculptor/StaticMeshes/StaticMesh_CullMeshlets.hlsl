#include "SculptorShader.hlsli"
#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(SMCullMeshletsDS, 4)]]


int ToInt8Value(uint bits)
{
    return int(bits & 127) - int(bits & 128);
}


void UnpackConeAxisAndCutoff(uint packedData, out float3 coneAxis, out float coneCutoff)
{
    coneCutoff = float(ToInt8Value(packedData >> 24)) / 127.f;
    coneAxis.x = float(ToInt8Value(packedData >> 16)) / 127.f;
    coneAxis.y = float(ToInt8Value(packedData >>  8)) / 127.f;
    coneAxis.z = float(ToInt8Value(packedData      )) / 127.f;
}


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

    const uint entityIdx = u_visibleInstances[batchElementIdx].entityIdx;
    const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];
    const float4x4 entityTransform = entityData.transform;

    const SubmeshGPUData submesh = u_submeshes[submeshIdx];
    
    for (uint localMeshletIdx = input.localID.x; localMeshletIdx < submesh.meshletsNum; localMeshletIdx += WORKLOAD_SIZE)
    {
        const uint globalMeshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;

        const MeshletGPUData meshlet = u_meshlets[globalMeshletIdx];
 
        bool isMeshletVisible = true;

        const float3 meshletBoundingSphereCenter = mul(entityTransform, float4(meshlet.boundingSphereCenter, 1.f)).xyz;
        const float meshletBoundingSphereRadius = meshlet.boundingSphereRadius * entityData.uniformScale;

        float3 coneAxis;
        float coneCutoff;
        UnpackConeAxisAndCutoff(meshlet.packedConeAxisAndCutoff, coneAxis, coneCutoff);

        const float3x3 entityRotationAndScale = float3x3(entityTransform[0].xyz, entityTransform[1].xyz, entityTransform[2].xyz);
        
        // In some cases axis is set to 0 vector (for example sometimes with planes)
        // we cannot normalize vertor then, and in most of those cases meshlet is visible anyway, so we don't do culling then
        if(any(coneAxis != 0.f))
        {
            coneAxis = normalize(mul(entityRotationAndScale, coneAxis));

            isMeshletVisible = isMeshletVisible && IsConeVisible(meshletBoundingSphereCenter, meshletBoundingSphereRadius, coneAxis, coneCutoff, u_sceneView.viewLocation);
            isMeshletVisible = isMeshletVisible && IsSphereInFrustum(u_cullingData.cullingPlanes, meshletBoundingSphereCenter, meshletBoundingSphereRadius);
        }

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