#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(RenderViewDS, 3)]]
[[descriptor_set(DepthCullingDS, 4)]]

[[descriptor_set(SMCullSubmeshesDS, 5)]]

#include "StaticMeshes/StaticMesh_Workload.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


[numthreads(64, 1, 1)]
void CullSubmeshesCS(CS_INPUT input)
{
    const uint batchElementIdx = input.globalID.x;

    if(batchElementIdx < u_batchData.elementsNum)
    {
        const uint submeshGlobalIdx = u_batchElements[batchElementIdx].submeshGlobalIdx;

        const SubmeshGPUData submesh = u_submeshes[submeshGlobalIdx];
        
        const uint entityIdx = u_batchElements[batchElementIdx].entityIdx;
        const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];
        const float4x4 entityTransform = entityData.transform;

        const float3 submeshBoundingSphereCenter = mul(entityTransform, float4(submesh.boundingSphereCenter, 1.f)).xyz;
        const float submeshBoundingSphereRadius = submesh.boundingSphereRadius * entityData.uniformScale;
        
        bool isSubmeshVisible = IsSphereInFrustum(u_cullingData.cullingPlanes, submeshBoundingSphereCenter, submeshBoundingSphereRadius);
        
        if(isSubmeshVisible)
        {
            const float3 submeshCenterVS = mul(u_sceneView.viewMatrix, float4(submeshBoundingSphereCenter, 1.f)).xyz;

            const float near = GetNearPlane(u_sceneView);
            const float p01 = u_sceneView.projectionMatrix[0][1];
            const float p12 = u_sceneView.projectionMatrix[1][2];

            const float2 hiZRes = u_depthCullingParams.hiZResolution;
            isSubmeshVisible = !IsSphereBehindHiZ(u_hiZTexture, u_hiZSampler, hiZRes, submeshCenterVS, submeshBoundingSphereRadius, near, p01, p12);
        }
      
        if(isSubmeshVisible)
        {
            const uint2 submeshVisibleBallot = WaveActiveBallot(isSubmeshVisible).xy;
        
            const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(u_dispatchVisibleBatchElemsParams[0], visibleSubmeshesNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

            u_visibleBatchElements[outputBufferIdx] = u_batchElements[batchElementIdx];
        }
    }

    if(input.globalID.x == 0)
    {
        u_dispatchVisibleBatchElemsParams[1] = 1;
        u_dispatchVisibleBatchElemsParams[2] = 1;
    }
}
