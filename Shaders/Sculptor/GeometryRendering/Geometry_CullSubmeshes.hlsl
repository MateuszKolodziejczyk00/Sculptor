#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryDefines.hlsli"

#if GEOMETRY_PASS_IDX != SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS && GEOMETRY_PASS_IDX != SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	#error "Invalid geometry pass index"
#endif

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]

[[shader_params(GeometryGPUBatchData, PARAMS_GEOMETRY_BATCH)]]

[[shader_params(GeometryCullingParams, PARAMS_GEOMETRY_CULLING)]]

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
[[shader_params(GeometryCullSubmeshes_VisibleGeometryPassParams, PASS)]]
#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
[[shader_params(GeometryCullSubmeshes_DisoccludedGeometryPassParams, PASS)]]
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
		outputOccludedBatchElemIdx = PASS->occludedBatchElementsCount.AtomicAdd(0u, occludedSubmeshesNum);

		const uint newOccludedSubmeshesCount = outputOccludedBatchElemIdx + occludedSubmeshesNum;
		const uint groupsToDispatch = (newOccludedSubmeshesCount + 63u) / 64u;

		PASS->dispatchOccludedElementsCommand.Cast<uint>().AtomicMax(0, groupsToDispatch);
	}
	outputOccludedBatchElemIdx = WaveReadLaneFirst(outputOccludedBatchElemIdx) + GetCompactedIndex(submeshOccludedBallot, WaveGetLaneIndex());

	PASS->occludedBatchElements[outputOccludedBatchElemIdx] = occludedBatchElem;
}
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS


void AppendDrawCommand(in GeometryDrawMeshTaskCommand drawCommand)
{
	const uint2 submeshVisibleBallot = WaveActiveBallot(true).xy;
	const uint visibleSubmeshesNum = countbits(submeshVisibleBallot.x) + countbits(submeshVisibleBallot.y);
	uint outputCommandIdx = 0;
	if (WaveIsFirstLane())
	{
		outputCommandIdx = PASS->drawCommandsCount.AtomicAdd(0u, visibleSubmeshesNum);
	}
	outputCommandIdx = WaveReadLaneFirst(outputCommandIdx) + GetCompactedIndex(submeshVisibleBallot, WaveGetLaneIndex());

	PASS->drawCommands[outputCommandIdx] = drawCommand;
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
	if(input.globalID.x >= PASS->occludedBatchElementsCount[0])
	{
		return;
	}

	const OccludedBatchElement occludedBatchElem = PASS->occludedBatchElements[input.globalID.x];
	const uint batchElementIdx = occludedBatchElem.batchElemIdx;
#endif // GEOMETRY_PASS_IDX

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
	if(input.globalID.x == 0)
	{
		PASS->dispatchOccludedElementsCommand[0].dispatchGroupsX = 0u;
		PASS->dispatchOccludedElementsCommand[0].dispatchGroupsY = 1u;
		PASS->dispatchOccludedElementsCommand[0].dispatchGroupsZ = 1u;
	}
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS

	if(batchElementIdx < PARAMS_GEOMETRY_BATCH->elementsNum)
	{
		const GeometryBatchElement batchElement = PARAMS_GEOMETRY_BATCH->batchElements[batchElementIdx];
		const SubmeshGPUData submesh            = batchElement.submeshPtr.Load();
		const RenderEntityGPUData entityData    = batchElement.entityPtr.Load();

		const float3 submeshBoundingSphereCenter = mul(entityData.transform, float4(submesh.boundingSphereCenter, 1.f)).xyz;
		const float submeshBoundingSphereRadius = submesh.boundingSphereRadius * entityData.uniformScale;

		bool isSubmeshVisible = false;

		const Sphere submeshBoundingSphere = Sphere::Create(submeshBoundingSphereCenter, submeshBoundingSphereRadius);

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS

		const bool isSubmeshInFrustum = IsSphereInFrustum(VIEW->cullingData.cullingPlanes, submeshBoundingSphere.center, submeshBoundingSphere.radius);

		if(isSubmeshInFrustum)
		{
			bool isOccluded = false;
			if(PARAMS_GEOMETRY_CULLING->hasHistoryHiZ)
			{
				const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(PARAMS_GEOMETRY_CULLING->historyHiZTexture, PARAMS_GEOMETRY_CULLING->historyHiZResolution, VIEW->prevFrameSceneView);
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

		const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(PARAMS_GEOMETRY_CULLING->hiZTexture, PARAMS_GEOMETRY_CULLING->hiZResolution, VIEW->sceneView);
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
