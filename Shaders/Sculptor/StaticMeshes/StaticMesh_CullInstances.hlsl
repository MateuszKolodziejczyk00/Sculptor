#include "SculptorShader.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(SMCullInstancesDS, 4)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void CullInstancesCS(CS_INPUT input)
{
    const uint batchElementIdx = input.globalID.x;

    if(batchElementIdx < u_batchData.elementsNum)
    {
        const uint meshIdx = u_batchElements[batchElementIdx].staticMeshIdx;
        const StaticMeshGPUData staticMesh = u_staticMeshes[meshIdx];

        const uint entityIdx = u_batchElements[batchElementIdx].entityIdx;
        const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];
        const float4x4 entityTransform = entityData.transform;

        const float3 boundingSphereCenter = mul(entityTransform, float4(staticMesh.boundingSphereCenter, 1.f)).xyz;
        const float boundingSphereRadius = staticMesh.boundingSphereRadius * entityData.uniformScale;

        bool isInstanceVisible = true;
        for(int planeIdx = 0; planeIdx < 4; ++planeIdx)
        {
            isInstanceVisible = isInstanceVisible && dot(u_cullingData.cullingPlanes[planeIdx], float4(boundingSphereCenter, 1.f)) > -boundingSphereRadius;
        }

        if (isInstanceVisible)
        {
            const uint2 instanceVisibleBallot = WaveActiveBallot(isInstanceVisible).xy;
        
            const uint visibleInstancesNum = countbits(instanceVisibleBallot.x) + countbits(instanceVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(u_dispatchInstancesParams[0], visibleInstancesNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(instanceVisibleBallot, WaveGetLaneIndex());

            u_visibleInstances[outputBufferIdx] = u_batchElements[batchElementIdx];
        }
    }
    
    if(input.globalID.x == 0)
    {
        u_dispatchInstancesParams[1] = 1;
        u_dispatchInstancesParams[2] = 1;
    }
}
