#include "SculptorShader.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

[[descriptor_set(GeometryBatchDS, 3)]]

[[descriptor_set(VisCullingDS, 4)]]

[[descriptor_set(GeometryCullSubmeshesDS, 5)]]

#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define SPT_GEOM_PASS_IDX_VISIBLE_LAST_FRAME        0
#define SPT_GEOM_PASS_IDX_DISOCCLUDED_CURRENT_FRAME 1
#define SPT_GEOM_PASSES_NUM                         2


[numthreads(64, 1, 1)]
void CullSubmeshesCS(CS_INPUT input)
{
	const uint batchElementIdx = input.globalID.x;

	if(batchElementIdx < u_batchData.elementsNum)
	{
		const GeometryBatchElement batchElement = u_batchElements[batchElementIdx];
		const SubmeshGPUData submesh            = u_submeshes[batchElement.submeshGlobalIdx];
		const RenderEntityGPUData entityData    = u_renderEntitiesData[batchElement.entityIdx];

		const float3 submeshBoundingSphereCenter = mul(entityData.transform, float4(submesh.boundingSphereCenter, 1.f)).xyz;
		const float submeshBoundingSphereRadius = submesh.boundingSphereRadius * entityData.uniformScale;

		bool isSubmeshVisible = IsSphereInFrustum(u_cullingData.cullingPlanes, submeshBoundingSphereCenter, submeshBoundingSphereRadius);

		SPT_CHECK_MSG(u_visCullingParams.passIdx < SPT_GEOM_PASSES_NUM, "Invalid geom pass idx ({})", u_visCullingParams.passIdx);

		const bool wantsOcclusionCulling = (u_visCullingParams.passIdx != SPT_GEOM_PASS_IDX_VISIBLE_LAST_FRAME || u_visCullingParams.hasHistoryHiZ);

		if(isSubmeshVisible && wantsOcclusionCulling)
		{
			// @Warning: Don't merge these two if statements into one (oclusionCullingProcessor cannot be shared)
			// Otherwise, DXC generates invalid spir-v
			if(u_visCullingParams.passIdx == SPT_GEOM_PASS_IDX_VISIBLE_LAST_FRAME)
			{
				const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(u_historyHiZTexture, u_visCullingParams.historyHiZResolution, u_hiZSampler, u_prevFrameSceneView);
				isSubmeshVisible = occlusionCullingProcessor.DoCulling(Sphere::Create(submeshBoundingSphereCenter, submeshBoundingSphereRadius));
			}
			else //if(passIdx == SPT_GEOM_PASS_IDX_DISOCCLUDED_CURRENT_FRAME)
			{
				const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(u_hiZTexture, u_visCullingParams.hiZResolution, u_hiZSampler, u_sceneView);
				isSubmeshVisible = occlusionCullingProcessor.DoCulling(Sphere::Create(submeshBoundingSphereCenter, submeshBoundingSphereRadius));
			}
		}

		if(isSubmeshVisible)
		{
			const uint2 submeshVisibleBallot = WaveActiveBallot(isSubmeshVisible).xy;
		
			const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y);

			uint outputCommandIdx = 0;
			if (WaveIsFirstLane())
			{
				InterlockedAdd(u_drawCommandsCount[0], visibleSubmeshesNum, outputCommandIdx);
			}

			outputCommandIdx = WaveReadLaneFirst(outputCommandIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

			const uint submeshMeshletsNum = submesh.meshletsNum;
			const uint taskGroupsNum = (submeshMeshletsNum + 31u) / 32u;

			GeometryDrawMeshTaskCommand drawCommand;
			drawCommand.dispatchGroupsX = taskGroupsNum;
			drawCommand.dispatchGroupsY = 1u;
			drawCommand.dispatchGroupsZ = 1u;
			drawCommand.batchElemIdx    = batchElementIdx;
			u_drawCommands[outputCommandIdx] = drawCommand;
		}
	}
}
