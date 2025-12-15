#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryDefines.hlsli"

#if GEOMETRY_PASS_IDX != SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS && GEOMETRY_PASS_IDX != SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	#error "Invalid geometry pass index"
#endif

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[descriptor_set(GeometryBatchDS)]]

[[descriptor_set(VisCullingDS)]]

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
[[descriptor_set(GeometryCullSubmeshes_VisibleGeometryPassDS)]]
#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
[[descriptor_set(GeometryCullSubmeshes_DisoccludedGeometryPassDS)]]
#endif // GEOMETRY_PASS_IDX

#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"


#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
void AppendOccludedBatchElement(in OccludedBatchElement occludedBatchElem)
{
	const uint2 submeshOccludedBallot = WaveActiveBallot(true).xy;
	const uint occludedSubmeshesNum = countbits(submeshOccludedBallot.x) + countbits(submeshOccludedBallot.y);
	uint outputOccludedBatchElemIdx = 0;
	if (WaveIsFirstLane())
	{
		InterlockedAdd(u_occludedBatchElementsCount[0], occludedSubmeshesNum, outputOccludedBatchElemIdx);

		const uint newOccludedSubmeshesCount = outputOccludedBatchElemIdx + occludedSubmeshesNum;
		const uint groupsToDispatch = (newOccludedSubmeshesCount + 63u) / 64u;

		uint prevGroups = 0;
		InterlockedMax(u_dispatchOccludedElementsCommand[0].dispatchGroupsX, groupsToDispatch, prevGroups);
	}
	outputOccludedBatchElemIdx = WaveReadLaneFirst(outputOccludedBatchElemIdx) + GetCompactedIndex(submeshOccludedBallot, WaveGetLaneIndex());

	u_occludedBatchElements[outputOccludedBatchElemIdx] = occludedBatchElem;
}
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS


void AppendDrawCommand(in GeometryDrawMeshTaskCommand drawCommand)
{
	const uint2 submeshVisibleBallot = WaveActiveBallot(true).xy;
	const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y);
	uint outputCommandIdx = 0;
	if (WaveIsFirstLane())
	{
		InterlockedAdd(u_drawCommandsCount[0], visibleSubmeshesNum, outputCommandIdx);
	}
	outputCommandIdx = WaveReadLaneFirst(outputCommandIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

	u_drawCommands[outputCommandIdx] = drawCommand;
}


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void CullSubmeshesCS(CS_INPUT input)
{
#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
	const uint batchElementIdx = input.globalID.x;
#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	if(input.globalID.x >= u_occludedBatchElementsCount[0])
	{
		return;
	}

	const OccludedBatchElement occludedBatchElem = u_occludedBatchElements[input.globalID.x];
	const uint batchElementIdx = occludedBatchElem.batchElemIdx;
#endif // GEOMETRY_PASS_IDX

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
	if(input.globalID.x == 0)
	{
		u_dispatchOccludedElementsCommand[0].dispatchGroupsX = 0u;
		u_dispatchOccludedElementsCommand[0].dispatchGroupsY = 1u;
		u_dispatchOccludedElementsCommand[0].dispatchGroupsZ = 1u;
	}
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS

	if(batchElementIdx < u_batchData.elementsNum)
	{
		const GeometryBatchElement batchElement = u_batchElements[batchElementIdx];
		const SubmeshGPUData submesh            = batchElement.submeshPtr.Load();
		const RenderEntityGPUData entityData    = batchElement.entityPtr.Load();

		const float3 submeshBoundingSphereCenter = mul(entityData.transform, float4(submesh.boundingSphereCenter, 1.f)).xyz;
		const float submeshBoundingSphereRadius = submesh.boundingSphereRadius * entityData.uniformScale;

		bool isSubmeshVisible = false;

		const Sphere submeshBoundingSphere = Sphere::Create(submeshBoundingSphereCenter, submeshBoundingSphereRadius);

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS

		const bool isSubmeshInFrustum = IsSphereInFrustum(u_cullingData.cullingPlanes, submeshBoundingSphere.center, submeshBoundingSphere.radius);

		if(isSubmeshInFrustum)
		{
			bool isOccluded = false;
			if(u_visCullingParams.hasHistoryHiZ)
			{
				const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(u_historyHiZTexture, u_visCullingParams.historyHiZResolution, u_hiZSampler, u_prevFrameSceneView);
				isOccluded = !occlusionCullingProcessor.DoCulling(submeshBoundingSphere);
			}

			if(isOccluded)
			{
				OccludedBatchElement occludedBatchElem;
				occludedBatchElem.batchElemIdx = batchElementIdx;
				AppendOccludedBatchElement(occludedBatchElem);
			}

			isSubmeshVisible = !isOccluded;
		}

#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS

		const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(u_hiZTexture, u_visCullingParams.hiZResolution, u_hiZSampler, u_sceneView);
		isSubmeshVisible = occlusionCullingProcessor.DoCulling(submeshBoundingSphere);

#endif // GEOMETRY_PASS_IDX

		if(isSubmeshVisible)
		{
			const uint taskGroupsNum = (submesh.meshlets.GetSize() + 31u) / 32u;

			GeometryDrawMeshTaskCommand drawCommand;
			drawCommand.dispatchGroupsX = taskGroupsNum;
			drawCommand.dispatchGroupsY = 1u;
			drawCommand.dispatchGroupsZ = 1u;
			drawCommand.batchElemIdx    = batchElementIdx;

			AppendDrawCommand(drawCommand);
		}
	}
}
