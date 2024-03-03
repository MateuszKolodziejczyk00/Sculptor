#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(StaticMeshBatchDS, 2)]]
[[descriptor_set(SMShadowMapCullingDS, 3)]]

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
        const uint entityIdx = u_batchElements[batchElementIdx].entityIdx;
        const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];
        const float4x4 entityTransform = entityData.transform;

        const uint submeshGlobalIdx = u_batchElements[batchElementIdx].submeshGlobalIdx;
        const SubmeshGPUData submesh = u_submeshes[submeshGlobalIdx];

        const float3 submeshBoundingSphereCenter = mul(entityTransform, float4(submesh.boundingSphereCenter, 1.f)).xyz;
        const float submeshBoundingSphereRadius = submesh.boundingSphereRadius * entityData.uniformScale;
        
#define CULL_SHADOW_MAP(idx) \
        if(idx < u_shadowMapsData.facesNum) \
        { \
            const bool isSubmeshVisible = IsSphereInFrustum(u_viewsCullingData[idx].cullingPlanes, submeshBoundingSphereCenter, submeshBoundingSphereRadius); \
\
            if (isSubmeshVisible) \
            { \
                const uint2 submeshVisibleBallot = WaveActiveBallot(isSubmeshVisible).xy; \
\
                const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y); \
\
                uint outputBufferIdx = 0; \
                if (WaveIsFirstLane()) \
                { \
                    InterlockedAdd(u_drawsCount[idx], visibleSubmeshesNum, outputBufferIdx); \
                } \
\
                outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex()); \
\
                u_drawCommandsFace##idx[outputBufferIdx].vertexCount = submesh.indicesNum; \
                u_drawCommandsFace##idx[outputBufferIdx].instanceCount = 1; \
                u_drawCommandsFace##idx[outputBufferIdx].firstVertex = 0; \
                u_drawCommandsFace##idx[outputBufferIdx].firstInstance = 0; \
                u_drawCommandsFace##idx[outputBufferIdx].batchElementIdx = batchElementIdx; \
            } \
        }

        CULL_SHADOW_MAP(0);
        CULL_SHADOW_MAP(1);
        CULL_SHADOW_MAP(2);
        CULL_SHADOW_MAP(3);
        CULL_SHADOW_MAP(4);
        CULL_SHADOW_MAP(5);
    }
}
