#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryDefines.hlsli"


[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(GeometryCullingDS)]]
[[descriptor_set(GeometryBatchDS)]]


#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
[[descriptor_set(GeometryDrawMeshes_VisibleGeometryPassDS)]]
#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
[[descriptor_set(GeometryDrawMeshes_DisoccludedGeometryPassDS)]]
#else
[[descriptor_set(GeometryDrawMeshes_DisoccludedMeshletsPassDS)]]
#endif // GEOMETRY_PASS_IDX


#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"
#include "SceneRendering/GPUScene.hlsli"


#if SPT_MATERIAL_ENABLED && CUSTOM_OPACITY
#define MATERIAL_CAN_DISCARD 1
#include "Materials/MaterialSystem.hlsli"
#else 
#define MATERIAL_CAN_DISCARD 0
#define FRAGMENT_SHADER_NEEDS_MATERIAL 0
#endif // SPT_MATERIAL_ENABLED && CUSTOM_OPACITY

#define TS_GROUP_SIZE 32


#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	#define MESHLETS_SHARE_ENTITY 1
#else
	#define MESHLETS_SHARE_ENTITY 0
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS


#define GEOMETRY_PIPELINE_USE_MATERIAL MATERIAL_CAN_DISCARD


#ifdef GEOMETRY_PIPELINE_SHADER
#define GEOMETRY_PIPELINE_DEFINITIONS_PASS
#include GEOMETRY_PIPELINE_SHADER
#undef GEOMETRY_PIPELINE_DEFINITIONS_PASS
#include "GeometryRendering/GeometryPipelineTypes.hlsli"
#include GEOMETRY_PIPELINE_SHADER
#else
#error "Missing GEOMETRY_PIPELINE_SHADER definition."
#endif


groupshared MeshPayload s_payload;


bool WasOccludedLastFrame(in Sphere sphere)
{
	const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(u_historyHiZTexture, u_visCullingParams.historyHiZResolution, u_hiZSampler, u_prevFrameSceneView);
	return !occlusionCullingProcessor.DoCulling(sphere);
}


bool IsOccludedCurrentFrame(in Sphere sphere)
{
	const HiZCullingProcessor occlusionCullingProcessor = HiZCullingProcessor::Create(u_hiZTexture, u_visCullingParams.hiZResolution, u_hiZSampler, u_sceneView);
	return !occlusionCullingProcessor.DoCulling(sphere);
}


#ifdef DS_GeometryDrawMeshes_VisibleGeometryPassDS
void AppendOccludedMeshlet(in OccludedMeshletData occludedMeshlet)
{
	const uint2 meshletOccludedBallot = WaveActiveBallot(true).xy;
	const uint occludedmeshletsNum = countbits(meshletOccludedBallot.x) + countbits(meshletOccludedBallot.y);
	uint outputOccludedMeshletIdx = 0;
	if (WaveIsFirstLane())
	{
		InterlockedAdd(u_occludedMeshletsCount[0], occludedmeshletsNum, outputOccludedMeshletIdx);

		const uint occludedMeshletsNum = outputOccludedMeshletIdx + occludedmeshletsNum;
		const uint requiredOccludedMeshletsGroups = (occludedMeshletsNum + TS_GROUP_SIZE - 1) / TS_GROUP_SIZE;
		uint prevGroups = 0;
		InterlockedMax(u_occludedMeshletsDispatchCommand[0].dispatchGroupsX, requiredOccludedMeshletsGroups, prevGroups);
	}
	outputOccludedMeshletIdx = WaveReadLaneFirst(outputOccludedMeshletIdx) + GetCompactedIndex(meshletOccludedBallot, WaveGetLaneIndex());

	u_occludedMeshlets[outputOccludedMeshletIdx] = occludedMeshlet;
}
#endif // DS_GeometryDrawMeshes_VisibleGeometryPassDS


struct TSInput
{
	uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
	[[vk::builtin("DrawIndex")]] uint drawCommandIndex : DRAW_INDEX;
};


[numthreads(TS_GROUP_SIZE, 1, 1)]
void GeometryPipeline_TS(in TSInput input)
{
#if !MESHLETS_SHARE_ENTITY
	s_payload.numCommands = 0;
#endif // !MESHLETS_SHARE_ENTITY

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	const GeometryDrawMeshTaskCommand drawCommand = u_drawCommands[input.drawCommandIndex];
	const uint batchElementIdx = drawCommand.batchElemIdx;
	const uint localMeshletIdx = input.globalID.x;
	const uint groupFirstLocalMesletIdx = input.groupID.x * TS_GROUP_SIZE;
#else
	if(input.globalID.x >= u_occludedMeshletsCount[0])
	{
		return;
	}

	const OccludedMeshletData occludedMeshlet = u_occludedMeshlets[input.globalID.x];
	const uint batchElementIdx = occludedMeshlet.batchElemIdx;
	const uint localMeshletIdx = occludedMeshlet.localMeshletIdx;
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
	if(input.globalID.x == 0)
	{
		u_occludedMeshletsDispatchCommand[0].dispatchGroupsX = 0u;
		u_occludedMeshletsDispatchCommand[0].dispatchGroupsY = 1u;
		u_occludedMeshletsDispatchCommand[0].dispatchGroupsZ = 1u;
	}
#endif	// GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS

	const GeometryBatchElement batchElement = u_batchElements[batchElementIdx];
	const SubmeshGPUData submesh            = batchElement.submeshPtr.Load();
	const RenderEntityGPUData entityData    = batchElement.entityPtr.Load();

#if MESHLETS_SHARE_ENTITY
	s_payload.batchElementIdx = batchElementIdx;

	uint visibleMeshletsNum = 0;
#endif // MESHLETS_SHARE_ENTITY

	GroupMemoryBarrierWithGroupSync();

	if(localMeshletIdx < submesh.meshlets.GetSize())
	{
		const MeshletGPUData meshlet = submesh.meshlets[localMeshletIdx];
 
		bool isMeshletVisible = true;

		const float3 meshletBoundingSphereCenter = mul(entityData.transform, float4(meshlet.boundingSphereCenter, 1.f)).xyz;
		const float meshletBoundingSphereRadius = meshlet.boundingSphereRadius * entityData.uniformScale;

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
		isMeshletVisible = IsSphereInFrustum(u_cullingData.cullingPlanes, meshletBoundingSphereCenter, meshletBoundingSphereRadius);

#ifndef DOUBLE_SIDED
		if(isMeshletVisible)
		{
			float3 coneAxis;
			float coneCutoff;
			UnpackConeAxisAndCutoff(meshlet.packedConeAxisAndCutoff, coneAxis, coneCutoff);

			const float3x3 entityRotationAndScale = float3x3(entityData.transform[0].xyz, entityData.transform[1].xyz, entityData.transform[2].xyz);
			
			// In some cases axis is set to 0 vector (for example sometimes with planes)
			// we cannot normalize vector then, and in most of those cases meshlet is visible anyway, so we don't do culling then
			if(any(coneAxis != 0.f))
			{
				coneAxis = normalize(mul(entityRotationAndScale, coneAxis));

				isMeshletVisible = isMeshletVisible && IsConeVisible(meshletBoundingSphereCenter, meshletBoundingSphereRadius, coneAxis, coneCutoff, u_sceneView.viewLocation);
			}
		}
#endif // DOUBLE_SIDED
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS

		if(isMeshletVisible)
		{
			const Sphere meshletBoundingSphere = Sphere::Create(meshletBoundingSphereCenter, meshletBoundingSphereRadius);

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS

			const bool isOccluded = WasOccludedLastFrame(meshletBoundingSphere);

			if(isOccluded)
			{
				OccludedMeshletData occludedMeshlet;
				occludedMeshlet.batchElemIdx    = batchElementIdx;
				occludedMeshlet.localMeshletIdx = localMeshletIdx;
				AppendOccludedMeshlet(occludedMeshlet);
			}

			isMeshletVisible = !isOccluded;
#else

			isMeshletVisible = !IsOccludedCurrentFrame(meshletBoundingSphere);

#endif // GEOMETRY_PASS_IDX
		}

		uint compactedLocalVisibleIdx = 0xffffffff;
		if(isMeshletVisible)
		{
#if MESHLETS_SHARE_ENTITY
			const uint thisThreadVisibilityBit = 1u << input.localID.x;
			const uint visibilityMask = WaveActiveBitOr(thisThreadVisibilityBit).x;
			visibleMeshletsNum = countbits(visibilityMask);
			compactedLocalVisibleIdx = GetCompactedIndex(uint2(visibilityMask, 0), input.localID.x);
			if (WaveIsFirstLane())
			{
				s_payload.validMeshletsMask    = visibilityMask;
				s_payload.firstLocalMeshletIdx = groupFirstLocalMesletIdx;
			}
#else
			const uint2 meshletVisibleBallot = WaveActiveBallot(true).xy;
			const uint visibleMeshletsNum = countbits(meshletVisibleBallot.x) + countbits(meshletVisibleBallot.y);
			uint outputCommandIdx = 0;
			if (WaveIsFirstLane())
			{
				InterlockedAdd(s_payload.numCommands, visibleMeshletsNum, outputCommandIdx);
			}
			compactedLocalVisibleIdx = GetCompactedIndex(meshletVisibleBallot, WaveGetLaneIndex());
			outputCommandIdx = WaveReadLaneFirst(outputCommandIdx) + compactedLocalVisibleIdx;

			s_payload.localMeshletIdx[outputCommandIdx] = localMeshletIdx;
			s_payload.batchElementIdx[outputCommandIdx] = batchElementIdx;
#endif // MESHLETS_SHARE_ENTITY
		}

		MeshletDispatchContext meshletContext;
		meshletContext.localID                  = input.localID.x;
		meshletContext.batchElement             = batchElement;
		meshletContext.submesh                  = submesh;
		meshletContext.entityData               = entityData;
		meshletContext.meshlet                  = meshlet;
		meshletContext.localMeshletIdx          = localMeshletIdx;
		meshletContext.isVisible                = isMeshletVisible;
		meshletContext.compactedLocalVisibleIdx = compactedLocalVisibleIdx;
#ifdef GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE
		DispatchMeshlet(meshletContext, s_payload.customData);
#else
		DispatchMeshlet(meshletContext);
#endif // GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE
	}

#if MESHLETS_SHARE_ENTITY
	visibleMeshletsNum = WaveActiveMax(visibleMeshletsNum);
	DispatchMesh(visibleMeshletsNum, 1, 1, s_payload);
#else
	DispatchMesh(s_payload.numCommands, 1, 1, s_payload);
#endif // MESHLETS_SHARE_ENTITY
}

uint PackTriangleVertices(in uint3 vertices, in uint triangleIdx)
{
	const uint maxIndex = 0x000003f;
	SPT_CHECK_MSG(all(vertices <= maxIndex), L"PackTriangleIndices: Invalid triangle vertices - {}", vertices);
	SPT_CHECK_MSG(triangleIdx <= maxIndex, L"PackTriangleIndices: Invalid triangle idx - {}", triangleIdx)
	return (triangleIdx << 18) | (vertices.x << 12) | (vertices.y << 6) | vertices.z;
}


uint3 UnpackTriangleVertices(in uint packedVertices, out uint triangleIdx)
{
	triangleIdx = (packedVertices >> 18) & 0x000003f;
	uint3 vertices;
	vertices.x = (packedVertices >> 12) & 0x000003f;
	vertices.y = (packedVertices >> 6) & 0x000003f;
	vertices.z = packedVertices & 0x000003f;
	return vertices;
}


#define MS_GROUP_SIZE 32
#define MAX_NUM_PRIMS 64
#define MAX_NUM_VERTS 64


struct MeshletTriangle
{
	float4 verticesCS[3]; // vertices in clip space
};


struct TriangleCullingParams
{
	float nearPlane;
	float2 viewportResolution;
	bool doubleSided;
};


bool IsTriangleVisible(in MeshletTriangle tri, in TriangleCullingParams cullingParams)
{
	const float3 clipX = float3(tri.verticesCS[0].x, tri.verticesCS[1].x, tri.verticesCS[2].x);
	const bool isInFrontOfPerspectivePlane = all(clipX > cullingParams.nearPlane);

	bool isTriangleVisible = true;

	const float3 verticesNDC[3] = 
	{
		tri.verticesCS[0].xyz / tri.verticesCS[0].w,
		tri.verticesCS[1].xyz / tri.verticesCS[1].w,
		tri.verticesCS[2].xyz / tri.verticesCS[2].w
	};

#ifndef DOUBLE_SIDED
	if(isTriangleVisible)
	{
		const float det = determinant(float3x3(tri.verticesCS[0].xyw, tri.verticesCS[1].xyw, tri.verticesCS[2].xyw));
		isTriangleVisible = det > 0.f;
	}
#endif // DOUBLE_SIDED

	if(isTriangleVisible && isInFrontOfPerspectivePlane)
	{
		float4 aabb;
		// Transform [-1, 1] to [0, resolution]
		aabb.xy = (min(verticesNDC[0].xy, min(verticesNDC[1].xy, verticesNDC[2].xy)) + 1.f) * 0.5f * cullingParams.viewportResolution;
		aabb.zw = (max(verticesNDC[0].xy, max(verticesNDC[1].xy, verticesNDC[2].xy)) + 1.f) * 0.5f * cullingParams.viewportResolution;

		// This is based on Niagara renderer created by Arseny Kapoulkine
		// Source: https://github.com/zeux/niagara/blob/master/src/shaders/meshlet.mesh.glsl
		// Original comment: "this can be set to 1/2^subpixelPrecisionBits"
		const float subpixelPrecision = 1.f / 256.f;

		// small primitive culling
		// Original comment: "this is slightly imprecise (doesn't fully match hw behavior and is both too loose and too strict)"
		isTriangleVisible = isTriangleVisible && (round(aabb.x - subpixelPrecision) != round(aabb.z) && round(aabb.y) != round(aabb.w + subpixelPrecision));
	}

	return isTriangleVisible;
}



[outputtopology("triangle")]
[numthreads(MS_GROUP_SIZE, 1, 1)]
void GeometryPipeline_MS(
	in uint localID : SV_GroupIndex,
	in uint3 groupID : SV_GroupID,
	in payload MeshPayload inPayload,
	out vertices PerVertexData outVerts[MAX_NUM_VERTS],
	out indices uint3 outTriangles[MAX_NUM_PRIMS],
	out primitives PerPrimitiveData outPrims[MAX_NUM_PRIMS]
	)
{
#if MESHLETS_SHARE_ENTITY
	// Each lane computes which group should take care of this command (meshlet)
	const uint groupIdxForCommands = ((1u << WaveGetLaneIndex()) & inPayload.validMeshletsMask) ? GetCompactedIndex(uint2(inPayload.validMeshletsMask, 0), WaveGetLaneIndex()) : MS_GROUP_SIZE;
	const uint commandIdx = WaveActiveMin(groupIdxForCommands == groupID.x ? WaveGetLaneIndex() : MS_GROUP_SIZE); // select only one command per group
	SPT_CHECK_MSG(commandIdx <= MS_GROUP_SIZE, L"Invalid command Idx");
	const uint localMeshletIdx   = inPayload.firstLocalMeshletIdx + commandIdx;
	const GeometryBatchElement batchElement = u_batchElements[inPayload.batchElementIdx];
#else
	const uint localMeshletIdx   = inPayload.localMeshletIdx[groupID.x];
	const uint batchElementIdx   = inPayload.batchElementIdx[groupID.x];
	const GeometryBatchElement batchElement = u_batchElements[batchElementIdx];
#endif // MESHLETS_SHARE_ENTITY

	const RenderEntityGPUData entityData = batchElement.entityPtr.Load();
	const SubmeshGPUData submesh         = batchElement.submeshPtr.Load();
	const MeshletGPUData meshlet         = submesh.meshlets[localMeshletIdx];

	SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

	const uint primitiveIndicesOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset;
	const uint locationsOffset = submesh.locationsOffset;
	const uint meshletGlobalVertexIndicesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;

	SPT_CHECK_MSG(meshlet.vertexCount <= MAX_NUM_VERTS, L"Invalid meshlet data (vertex count) - {}", meshlet.vertexCount);
	SPT_CHECK_MSG(meshlet.triangleCount <= MAX_NUM_PRIMS, L"Invalid meshlet data (primitives count) - {}", meshlet.triangleCount);

	// { lane_idx, lane_idx + MS_GROUP_SIZE } clip space vertices
	float4 localVerticesCS[2];

	for (uint meshletVertexIdx = WaveGetLaneIndex(); meshletVertexIdx < meshlet.vertexCount; meshletVertexIdx += MS_GROUP_SIZE)
	{
		const uint vertexIdx = UGB().LoadVertexIndex(meshletGlobalVertexIndicesOffset, meshletVertexIdx);
		SPT_CHECK_MSG(vertexIdx < submesh.indicesNum, L"Invalid vertex index - {}", vertexIdx);
		const float3 vertexLocation = UGB().LoadLocation(locationsOffset, vertexIdx);
		const float4 vertexCS = mul(u_sceneView.viewProjectionMatrix, mul(entityData.transform, float4(vertexLocation, 1.f)));
		outVerts[meshletVertexIdx].locationCS = vertexCS;
		localVerticesCS[(meshletVertexIdx >= MS_GROUP_SIZE)] = vertexCS;

#if GEOMETRY_PIPELINE_USE_MATERIAL
		const float2 normalizedUV = UGB().LoadNormalizedUV(submesh.uvsOffset, vertexIdx);
		outVerts[meshletVertexIdx].uv = submesh.uvsMin + normalizedUV * submesh.uvsRange;
#endif // GEOMETRY_PIPELINE_USE_MATERIAL
	}

	TriangleCullingParams cullingParams;
	cullingParams.nearPlane          = GetNearPlane(u_sceneView);
	cullingParams.viewportResolution = u_viewRenderingParams.renderingResolution;
	
	// Warning: each thread must iterate same number of times to make sure that WaveReadLaneAt works correctly
	for (uint groupTriangleIdx = 0; groupTriangleIdx < meshlet.triangleCount; groupTriangleIdx += MS_GROUP_SIZE)
	{
		const uint meshletTriangleIdx = min(groupTriangleIdx + localID, meshlet.triangleCount - 1);
		const uint3 triangleVertices = LoadTriangleVertexIndices(primitiveIndicesOffset, meshletTriangleIdx);
		outTriangles[meshletTriangleIdx] = triangleVertices;

		MeshletTriangle tri;
		[unroll]
		for(uint i = 0; i < 3; i++)
		{
			// We have to make both of those reads on every thread
			const float4 cs0 = WaveReadLaneAt(localVerticesCS[0], (triangleVertices[i] & 31)); // Read vertices in range [0, 31]
			const float4 cs1 = WaveReadLaneAt(localVerticesCS[1], (triangleVertices[i] & 31)); // Read vertices in range [32, 63]
			tri.verticesCS[i] = triangleVertices[i] < MS_GROUP_SIZE ? cs0 : cs1;
		}


		PerPrimitiveData primData;
#if SPT_MESH_SHADER
		primData.culled = groupTriangleIdx + localID >= meshlet.triangleCount || !IsTriangleVisible(tri, cullingParams);
#endif // SPT_MESH_SHADER
#if GEOMETRY_PIPELINE_USE_MATERIAL
		primData.materialDataID = uint(batchElement.materialDataHandle.id);
#endif // GEOMETRY_PIPELINE_USE_MATERIAL

		TriangleDispatchContext triDispatchContext;
		triDispatchContext.groupID            = groupID.x;
		triDispatchContext.localID            = localID.x;
		triDispatchContext.entityData         = entityData;
		triDispatchContext.submesh            = submesh;
		triDispatchContext.meshlet            = meshlet;
		triDispatchContext.meshletTriangleIdx = meshletTriangleIdx;
#ifdef GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE
		DispatchTriangle(triDispatchContext, inPayload.customData, primData);
#else
		DispatchTriangle(triDispatchContext, primData);
#endif // GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE

		outPrims[meshletTriangleIdx] = primData;
	}
}


#ifndef FRAGMENT_SHADER_OUTPUT_TYPE
#define FRAGMENT_SHADER_OUTPUT_TYPE void
#define DispatchFragment(...)
#endif // FRAGMENT_SHADER_OUTPUT_TYPE


FRAGMENT_SHADER_OUTPUT_TYPE GeometryPipeline_FS(in PerVertexData vertData, in PerPrimitiveData primData)
{
#if GEOMETRY_PIPELINE_USE_MATERIAL
	const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(MaterialDataHandle(primData.materialDataID));

#if MATERIAL_CAN_DISCARD
	MaterialEvaluationParameters materialEvalParams;
	materialEvalParams.uv = vertData.uv;

	const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);
	if(opacityOutput.shouldDiscard)
	{
		discard;
	}
#endif // MATERIAL_CAN_DISCARD
#endif // GEOMETRY_PIPELINE_USE_MATERIAL

	FragmentDispatchContext fragDispatchContext;
	fragDispatchContext.vertData = vertData;
	fragDispatchContext.primData = primData;
#if GEOMETRY_PIPELINE_USE_MATERIAL
	fragDispatchContext.materialData = materialData;
#endif // GEOMETRY_PIPELINE_USE_MATERIAL

	return DispatchFragment(fragDispatchContext);
}
