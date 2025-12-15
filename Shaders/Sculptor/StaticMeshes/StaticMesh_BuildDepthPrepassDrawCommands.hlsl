#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(StaticMeshBatchDS)]]

[[descriptor_set(SMDepthPrepassCullInstancesDS)]]

#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void BuildDrawCommandsCS(CS_INPUT input)
{
    const uint batchElementIdx = input.globalID.x;

    if(batchElementIdx < u_batchData.elementsNum)
    {
		const StaticMeshBatchElement batchElem = u_batchElements[batchElementIdx];
		const RenderEntityGPUData entityData = batchElem.entityPtr.Load();
        const float4x4 entityTransform = entityData.transform;

		const SubmeshGPUData submesh = batchElem.submeshPtr.Load();

        const float3 submeshBoundingSphereCenter = mul(entityTransform, float4(submesh.boundingSphereCenter, 1.f)).xyz;
        const float submeshBoundingSphereRadius = submesh.boundingSphereRadius * entityData.uniformScale;
        
        const bool isSubmeshVisible = IsSphereInFrustum(u_cullingData.cullingPlanes, submeshBoundingSphereCenter, submeshBoundingSphereRadius);

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
            
            u_drawCommands[outputBufferIdx].batchElementIdx = batchElementIdx;
        }
    }
}
