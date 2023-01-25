#include "SculptorShader.hlsli"
#include "Utils/Wave.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMProcessBatchForViewDS, 3)]]

[[descriptor_set(SMDepthPrepassCullInstancesDS, 4)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void BuildDrawCommandsCS(CS_INPUT input)
{
    const uint batchElementIdx = input.globalID.x;

    if(batchElementIdx < u_validBatchElementsNum[0])
    {
        const uint meshIdx = u_batchElements[batchElementIdx].staticMeshIdx;
        const StaticMeshGPUData staticMesh = u_staticMeshes[meshIdx];

        const uint entityIdx = u_batchElements[batchElementIdx].entityIdx;
        const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];
        const float4x4 entityTransform = entityData.transform;

        const float3 boundingSphereCenter = mul(entityTransform, float4(staticMesh.boundingSphereCenter, 1.f)).xyz;
        const float boundingSphereRadius = staticMesh.boundingSphereRadius * entityData.uniformScale;

        bool isInstanceVisible = true;
        for(uint planeIdx = 0; planeIdx < 4; ++planeIdx)
        {
            isInstanceVisible = isInstanceVisible && dot(u_cullingData.cullingPlanes[planeIdx], float4(boundingSphereCenter, 1.f)) > -boundingSphereRadius;
        }

        if (isInstanceVisible)
        {
            for (uint submeshIdx = 0; submeshIdx < staticMesh.submeshesNum; ++submeshIdx)
            {
                const uint submeshGlobalIdx = staticMesh.submeshesBeginIdx + submeshIdx;
                const SubmeshGPUData submesh = u_submeshes[submeshGlobalIdx];
                
                const bool isSubmeshVisible = true;

                if (isSubmeshVisible)
                {
                    const uint2 submeshVisibleBallot = WaveActiveBallot(isSubmeshVisible).xy;
                
                    const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y);

                    uint outputBufferIdx = 0;
                    if (WaveIsFirstLane())
                    {
                        InterlockedAdd(u_drawsCount[0], visibleSubmeshesNum, outputBufferIdx);
                    }

                    outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

                    u_drawCommands[outputBufferIdx].vertexCount     = submesh.indicesNum;
                    u_drawCommands[outputBufferIdx].instanceCount   = 1;
                    u_drawCommands[outputBufferIdx].firstVertex     = 0;
                    u_drawCommands[outputBufferIdx].firstInstance   = 0;

                    u_drawCommands[outputBufferIdx].batchElementIdx     = batchElementIdx;
                    u_drawCommands[outputBufferIdx].submeshGlobalIdx    = submeshGlobalIdx;
                    u_drawCommands[outputBufferIdx].padding0            = 0;
                    u_drawCommands[outputBufferIdx].padding1            = 0;
                }
            }
        }
    }
}
