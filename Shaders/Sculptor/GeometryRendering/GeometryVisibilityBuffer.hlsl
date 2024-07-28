#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryDefines.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

[[descriptor_set(GeometryBatchDS, 3)]]

[[descriptor_set(VisCullingDS, 4)]]

[[descriptor_set(GeometryDS, 5)]]

[[descriptor_set(MaterialsDS, 6)]]

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
[[descriptor_set(GeometryDrawMeshes_VisibleGeometryPassDS, 7)]]
#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
[[descriptor_set(GeometryDrawMeshes_DisoccludedGeometryPassDS, 7)]]
#else
[[descriptor_set(GeometryDrawMeshes_DisoccludedMeshletsPassDS, 7)]]
#endif // GEOMETRY_PASS_IDX


#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"


#if SPT_MATERIAL_ENABLED && SPT_MATERIAL_CUSTOM_OPACITY
#define MATERIAL_CAN_DISCARD 1
#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH
#else 
#define MATERIAL_CAN_DISCARD 0
#define FRAGMENT_SHADER_NEEDS_MATERIAL 0
#endif // SPT_MATERIAL_ENABLED && SPT_MATERIAL_CUSTOM_OPACITY


#define TS_GROUP_SIZE 32


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


#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	#define MESHLETS_SHARE_ENTITY 1
#else
	#define MESHLETS_SHARE_ENTITY 0
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS


#if !MESHLETS_SHARE_ENTITY
	#define ENCODED_MESHLET_COMMAND_TYPE uint4
#else
	#define ENCODED_MESHLET_COMMAND_TYPE uint2
#endif // !MESHLETS_SHARE_ENTITY


struct MeshletRenderCommand
{
	ENCODED_MESHLET_COMMAND_TYPE Encode()
	{
		ENCODED_MESHLET_COMMAND_TYPE data;
		data.x = localMeshletIdx;
		data.y = visibleMeshletIdx;

#if !MESHLETS_SHARE_ENTITY
		data.z = batchElementIdx;
#endif // !MESHLETS_SHARE_ENTITY

		return data;
	}

	static MeshletRenderCommand Decode(ENCODED_MESHLET_COMMAND_TYPE data)
	{
		MeshletRenderCommand command;
		command.localMeshletIdx   = data.x;
		command.visibleMeshletIdx = data.y;

#if !MESHLETS_SHARE_ENTITY
		command.batchElementIdx = data.z;
#endif // !MESHLETS_SHARE_ENTITY

		return command;
	}

	uint localMeshletIdx;
	uint visibleMeshletIdx;

#if !MESHLETS_SHARE_ENTITY
	uint batchElementIdx;
#endif // !MESHLETS_SHARE_ENTITY
};


struct MeshPayload
{

#if MESHLETS_SHARE_ENTITY
	uint batchElementIdx;

	uint firstVisibleMeshletIdx;
	uint firstLocalMeshletIdx;

	uint validMeshletsMask;
#else
	ENCODED_MESHLET_COMMAND_TYPE commands[TS_GROUP_SIZE];
	uint numCommands;
#endif // MESHLETS_SHARE_ENTITY
};


groupshared MeshPayload s_payload;


#if !MESHLETS_SHARE_ENTITY
void AppendMeshletRenderCommand(in GPUVisibleMeshlet visibleMeshletInfo, in uint batchElementIdx, in uint localMeshletIdx)
{
	uint outputCommandIdx = 0;
	const uint2 meshletVisibleBallot = WaveActiveBallot(true).xy;
	const uint visibleMeshletsNum = countbits(meshletVisibleBallot.x) + countbits(meshletVisibleBallot.y);
	uint visibleMeshletIdx = 0u;
	if (WaveIsFirstLane())
	{
		InterlockedAdd(s_payload.numCommands, visibleMeshletsNum, outputCommandIdx);
		InterlockedAdd(u_visibleMeshletsCount[0], visibleMeshletsNum, visibleMeshletIdx);
	}
	const uint localIdxOffset = GetCompactedIndex(meshletVisibleBallot, WaveGetLaneIndex());
	outputCommandIdx = WaveReadLaneFirst(outputCommandIdx) + localIdxOffset;
	visibleMeshletIdx = WaveReadLaneFirst(visibleMeshletIdx) + localIdxOffset;

	u_visibleMeshlets[visibleMeshletIdx] = visibleMeshletInfo;

	MeshletRenderCommand command;
	command.localMeshletIdx   = localMeshletIdx;
	command.visibleMeshletIdx = visibleMeshletIdx;
	
#if !MESHLETS_SHARE_ENTITY
	command.batchElementIdx   = batchElementIdx;
#endif // !MESHLETS_SHARE_ENTITY

	s_payload.commands[outputCommandIdx] = command.Encode();
}
#endif // !MESHLETS_SHARE_ENTITY


struct TSInput
{
	uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
	[[vk::builtin("DrawIndex")]] uint drawCommandIndex : DRAW_INDEX;
};


[numthreads(TS_GROUP_SIZE, 1, 1)]
void GeometryVisibility_TS(in TSInput input)
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
	const SubmeshGPUData submesh            = u_submeshes[batchElement.submeshGlobalIdx];
	const RenderEntityGPUData entityData    = u_renderEntitiesData[batchElement.entityIdx];

#if MESHLETS_SHARE_ENTITY
	s_payload.batchElementIdx = batchElementIdx;

	uint visibleMeshletsNum = 0;
#endif // MESHLETS_SHARE_ENTITY

	GroupMemoryBarrierWithGroupSync();

	if(localMeshletIdx < submesh.meshletsNum)
	{
		const uint globalMeshletIdx = submesh.meshletsBeginIdx + localMeshletIdx;

		const MeshletGPUData meshlet = u_meshlets[globalMeshletIdx];
 
		bool isMeshletVisible = true;

		const float3 meshletBoundingSphereCenter = mul(entityData.transform, float4(meshlet.boundingSphereCenter, 1.f)).xyz;
		const float meshletBoundingSphereRadius = meshlet.boundingSphereRadius * entityData.uniformScale;

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
		isMeshletVisible = IsSphereInFrustum(u_cullingData.cullingPlanes, meshletBoundingSphereCenter, meshletBoundingSphereRadius);

#ifndef SPT_MATERIAL_DOUBLE_SIDED
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
#endif // SPT_MATERIAL_DOUBLE_SIDED
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

		if(isMeshletVisible)
		{
#if MESHLETS_SHARE_ENTITY
			const uint thisThreadVisibilityBit = 1u << input.localID.x;
			const uint visibilityMask = WaveActiveBitOr(thisThreadVisibilityBit).x;
			visibleMeshletsNum = countbits(visibilityMask);
			const uint compactedVisibilityIdx = GetCompactedIndex(uint2(visibilityMask, 0), input.localID.x);
			uint visibleMeshletIdx = 0u;
			if (WaveIsFirstLane())
			{
				InterlockedAdd(u_visibleMeshletsCount[0], visibleMeshletsNum, visibleMeshletIdx);
				s_payload.validMeshletsMask      = visibilityMask;
				s_payload.firstVisibleMeshletIdx = visibleMeshletIdx;
				s_payload.firstLocalMeshletIdx   = groupFirstLocalMesletIdx;
			}

			visibleMeshletIdx = WaveReadLaneFirst(visibleMeshletIdx) + compactedVisibilityIdx;

			GPUVisibleMeshlet visibleMeshletInfo;
			visibleMeshletInfo.entityIdx        = batchElement.entityIdx;
			visibleMeshletInfo.meshletGlobalIdx = globalMeshletIdx;
			visibleMeshletInfo.submeshGlobalIdx = batchElement.submeshGlobalIdx;
			visibleMeshletInfo.materialDataID   = batchElement.materialDataID;
			visibleMeshletInfo.materialBatchIdx = batchElement.materialBatchIdx;

			u_visibleMeshlets[visibleMeshletIdx] = visibleMeshletInfo;
#else
			GPUVisibleMeshlet visibleMeshletInfo;
			visibleMeshletInfo.entityIdx        = batchElement.entityIdx;
			visibleMeshletInfo.meshletGlobalIdx = globalMeshletIdx;
			visibleMeshletInfo.submeshGlobalIdx = batchElement.submeshGlobalIdx;
			visibleMeshletInfo.materialDataID   = batchElement.materialDataID;
			visibleMeshletInfo.materialBatchIdx = batchElement.materialBatchIdx;
			AppendMeshletRenderCommand(visibleMeshletInfo, batchElementIdx, localMeshletIdx);
#endif // MESHLETS_SHARE_ENTITY
		}
	}

#if MESHLETS_SHARE_ENTITY
	visibleMeshletsNum = WaveActiveMax(visibleMeshletsNum);
	DispatchMesh(visibleMeshletsNum, 1, 1, s_payload);
#else
	DispatchMesh(s_payload.numCommands, 1, 1, s_payload);
#endif // MESHLETS_SHARE_ENTITY
}

struct OutputVertex
{
	float4 locationCS : SV_Position;
#if MATERIAL_CAN_DISCARD
	float2 uv : UV;
#endif // MATERIAL_CAN_DISCARD
};


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

#ifndef SPT_MATERIAL_DOUBLE_SIDED
	if(isTriangleVisible)
	{
		const float det = determinant(float3x3(tri.verticesCS[0].xyw, tri.verticesCS[1].xyw, tri.verticesCS[2].xyw));
		isTriangleVisible = det > 0.f;
	}
#endif // SPT_MATERIAL_DOUBLE_SIDED

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


struct PrimitiveOutput
{
	uint packedVisibilityInfo : PACKED_VISBILITY_INFO;

#if MATERIAL_CAN_DISCARD
	uint materialDataID : MATERIAL_DATA_ID;
#endif // MATERIAL_CAN_DISCARD

	bool culled               : SV_CullPrimitive;
};



[outputtopology("triangle")]
[numthreads(MS_GROUP_SIZE, 1, 1)]
void GeometryVisibility_MS(
	in uint localID : SV_GroupIndex,
	in uint3 groupID : SV_GroupID,
	in payload MeshPayload payload,
	out vertices OutputVertex outVerts[MAX_NUM_VERTS],
	out indices uint3 outTriangles[MAX_NUM_PRIMS],
	out primitives PrimitiveOutput outPrims[MAX_NUM_PRIMS]
	)
{
#if MESHLETS_SHARE_ENTITY
	// Each lane computes which group should take care of this command (meshlet)
	const uint groupIdxForCommands = ((1u << WaveGetLaneIndex()) & payload.validMeshletsMask) ? GetCompactedIndex(uint2(payload.validMeshletsMask, 0), WaveGetLaneIndex()) : MS_GROUP_SIZE;
	const uint commandIdx = WaveActiveMin(groupIdxForCommands == groupID.x ? WaveGetLaneIndex() : MS_GROUP_SIZE); // select only one command per group
	SPT_CHECK_MSG(commandIdx <= MS_GROUP_SIZE, L"Invalid command Idx");
	MeshletRenderCommand command;
	command.localMeshletIdx   = payload.firstLocalMeshletIdx + commandIdx;
	command.visibleMeshletIdx = payload.firstVisibleMeshletIdx + groupID.x;
#else
	const MeshletRenderCommand command = MeshletRenderCommand::Decode(payload.commands[groupID.x]);
#endif // MESHLETS_SHARE_ENTITY

#if MESHLETS_SHARE_ENTITY
	const GeometryBatchElement batchElement = u_batchElements[payload.batchElementIdx];

	const uint submeshGlobalIdx = batchElement.submeshGlobalIdx;
	const uint entityIdx        = batchElement.entityIdx;
#else
	const GeometryBatchElement batchElement = u_batchElements[command.batchElementIdx];

	const uint submeshGlobalIdx = batchElement.submeshGlobalIdx;
	const uint entityIdx        = batchElement.entityIdx;
#endif // MESHLETS_SHARE_ENTITY

	const SubmeshGPUData submesh         = u_submeshes[submeshGlobalIdx];
	const uint globalMeshletIdx          = submesh.meshletsBeginIdx + command.localMeshletIdx;
	const MeshletGPUData meshlet         = u_meshlets[globalMeshletIdx];
	const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];

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
		const uint vertexIdx = u_geometryData.Load<uint>(meshletGlobalVertexIndicesOffset + (meshletVertexIdx * 4));
		SPT_CHECK_MSG(vertexIdx < submesh.indicesNum, L"Invalid vertex index - {}", vertexIdx);
		const float3 vertexLocation = u_geometryData.Load<float3>(locationsOffset + vertexIdx * 12);
		const float4 vertexCS = mul(u_sceneView.viewProjectionMatrix, mul(entityData.transform, float4(vertexLocation, 1.f)));
		outVerts[meshletVertexIdx].locationCS = vertexCS;
		localVerticesCS[(meshletVertexIdx >= MS_GROUP_SIZE)] = vertexCS;

#if MATERIAL_CAN_DISCARD
		const float2 vertexUV = u_geometryData.Load<float2>(submesh.uvsOffset + vertexIdx * 8);
		outVerts[meshletVertexIdx].uv = vertexUV;
#endif // MATERIAL_CAN_DISCARD
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

		outPrims[meshletTriangleIdx].packedVisibilityInfo = PackVisibilityInfo(command.visibleMeshletIdx, meshletTriangleIdx);
		outPrims[meshletTriangleIdx].culled = groupTriangleIdx + localID >= meshlet.triangleCount || !IsTriangleVisible(tri, cullingParams);
#if MATERIAL_CAN_DISCARD
		outPrims[meshletTriangleIdx].materialDataID = uint(batchElement.materialDataID);
#endif // MATERIAL_CAN_DISCARD
	}
}


struct VIS_BUFFER_PS_OUT
{
	uint packedVisibilityInfo : SV_TARGET0;
};


VIS_BUFFER_PS_OUT GeometryVisibility_FS(in OutputVertex vertexInput, in PrimitiveOutput primData)
{
#if MATERIAL_CAN_DISCARD
    MaterialEvaluationParameters materialEvalParams;
    materialEvalParams.uv = vertexInput.uv;
    
    const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(uint16_t(primData.materialDataID));

    const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);
    if(opacityOutput.shouldDiscard)
    {
        discard;
    }
#endif // MATERIAL_CAN_DISCARD

	VIS_BUFFER_PS_OUT output;
	output.packedVisibilityInfo = primData.packedVisibilityInfo;
	return output;
}
