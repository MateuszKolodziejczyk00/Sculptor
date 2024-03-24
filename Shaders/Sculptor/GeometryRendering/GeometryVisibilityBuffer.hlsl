#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryCommon.h"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

[[descriptor_set(GeometryBatchDS, 3)]]

[[descriptor_set(VisCullingDS, 4)]]

[[descriptor_set(GeometryDS, 5)]]

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS
[[descriptor_set(GeometryDrawMeshes_VisibleGeometryPassDS, 6)]]
#elif GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
[[descriptor_set(GeometryDrawMeshes_DisoccludedGeometryPassDS, 6)]]
#else
[[descriptor_set(GeometryDrawMeshes_DisoccludedMeshletsPassDS, 6)]]
#endif // GEOMETRY_PASS_IDX


#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"


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
	#define ENCODED_MESHLET_COMMAND_TYPE uint2
	#define MESHLETS_SHARE_ENTITY 1
#else
	#define ENCODED_MESHLET_COMMAND_TYPE uint4
	#define MESHLETS_SHARE_ENTITY 0
#endif // GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS


struct MeshletRenderCommand
{
	ENCODED_MESHLET_COMMAND_TYPE Encode()
	{
		ENCODED_MESHLET_COMMAND_TYPE data;
		data.x = localMeshletIdx;
		data.y = visibleMeshletIdx;

#if !MESHLETS_SHARE_ENTITY
		data.z = entityIdx;
		data.w = submeshGlobalIdx;
#endif // !MESHLETS_SHARE_ENTITY

		return data;
	}

	static MeshletRenderCommand Decode(ENCODED_MESHLET_COMMAND_TYPE data)
	{
		MeshletRenderCommand command;
		command.localMeshletIdx   = data.x;
		command.visibleMeshletIdx = data.y;
#if !MESHLETS_SHARE_ENTITY
		command.entityIdx         = data.z;
		command.submeshGlobalIdx  = data.w;
#endif // !MESHLETS_SHARE_ENTITY

		return command;
	}

	uint localMeshletIdx;
	uint visibleMeshletIdx;

#if !MESHLETS_SHARE_ENTITY
	uint entityIdx;
	uint submeshGlobalIdx;
#endif // !MESHLETS_SHARE_ENTITY
};


struct MeshPayload
{
	//MeshletRenderCommand commands[TS_GROUP_SIZE];
	// looks like we can't use structs in payloads - validation layers are crashing when creating shader module
	// This is MeshletRenderCommand 
	ENCODED_MESHLET_COMMAND_TYPE commands[TS_GROUP_SIZE];
	//GeometryBatchElement batchElement;
#if MESHLETS_SHARE_ENTITY
	uint entityIdx;
	uint submeshGlobalIdx;
#endif // MESHLETS_SHARE_ENTITY
	uint numCommands;
};


groupshared MeshPayload s_payload;


void AppendMeshletRenderCommand(in GPUVisibleMeshlet visibleMeshletInfo, in GeometryBatchElement batchElement, in uint localMeshletIdx)
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
	command.entityIdx         = batchElement.entityIdx;
	command.submeshGlobalIdx  = batchElement.submeshGlobalIdx;
#endif // !MESHLETS_SHARE_ENTITY
	s_payload.commands[outputCommandIdx] = command.Encode();
}


struct TSInput
{
	uint3 globalID : SV_DispatchThreadID;
	[[vk::builtin("DrawIndex")]] uint drawCommandIndex : DRAW_INDEX;
};


[numthreads(TS_GROUP_SIZE, 1, 1)]
void GeometryVisibilityTS(in TSInput input)
{
	s_payload.numCommands = 0;

#if GEOMETRY_PASS_IDX == SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS || GEOMETRY_PASS_IDX == SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS
	const GeometryDrawMeshTaskCommand drawCommand = u_drawCommands[input.drawCommandIndex];
	const uint batchElementIdx = drawCommand.batchElemIdx;
	const uint localMeshletIdx = input.globalID.x;
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
	s_payload.entityIdx	       = batchElement.entityIdx;
	s_payload.submeshGlobalIdx = batchElement.submeshGlobalIdx;
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
			GPUVisibleMeshlet visibleMeshletInfo;
			visibleMeshletInfo.entityIdx          = batchElement.entityIdx;
			visibleMeshletInfo.meshletGlobalIdx   = globalMeshletIdx;
			visibleMeshletInfo.materialDataOffset = batchElement.materialDataOffset;
			visibleMeshletInfo.materialBatchIdx   = batchElement.materialBatchIdx;

			AppendMeshletRenderCommand(visibleMeshletInfo, batchElement, localMeshletIdx);
		}
	}

	DispatchMesh(s_payload.numCommands, 1, 1, s_payload);
}


uint3 LoadTriangleVertexIndices(uint meshletPrimitivesOffset, uint meshletTriangleIdx)
{
	const uint triangleStride = 3;
	const uint primitiveOffset = meshletPrimitivesOffset + meshletTriangleIdx * triangleStride;

	// Load multiple of 4
	const uint primitiveOffsetToLoad = primitiveOffset & 0xfffffffc;

	// Load uint2 to be sure that we will have all 3 indices
	const uint2 meshletPrimitivesIndices = u_geometryData.Load<uint2>(primitiveOffsetToLoad);

	uint primitiveIndicesByteOffset = primitiveOffset - primitiveOffsetToLoad;

	uint3 traingleIndices;

	for (int idx = 0; idx < 3; ++idx, ++primitiveIndicesByteOffset)
	{
		uint indices4;
		uint offset;
		
		if(primitiveIndicesByteOffset >= 4)
		{
			indices4 = meshletPrimitivesIndices[1];
			offset = primitiveIndicesByteOffset - 4;
		}
		else
		{
			indices4 = meshletPrimitivesIndices[0];
			offset = primitiveIndicesByteOffset;
		}

		traingleIndices[idx] = (indices4 >> (offset * 8)) & 0x000000ff;
	}

	return traingleIndices;
}

// This is a bunch of outparams for the Mesh shader. At least SV_Position must be present.
struct OutputVertex
{
	float4 locationCS : SV_Position;
};


uint PackTriangleVertices(uint3 vertices)
{
	const uint maxIndex = 0x000003ff;
	SPT_CHECK_MSG(all(vertices < maxIndex), "PackTriangleIndices: Invalid triangle vertices - {}", vertices);
	return (vertices.x << 20) | (vertices.y << 10) | vertices.z;
}


uint3 UnpackTriangleVertices(uint packedVertices)
{
	uint3 vertices;
	vertices.x = (packedVertices >> 20) & 0x000003ff;
	vertices.y = (packedVertices >> 10) & 0x000003ff;
	vertices.z = packedVertices & 0x000003ff;
	return vertices;
}


#define MS_GROUP_SIZE 32
#define MAX_NUM_PRIMS 64
#define MAX_NUM_VERTS 64


struct MSSharedData
{
	OutputVertex vertices[MAX_NUM_VERTS];
	uint triangles[MAX_NUM_PRIMS];
	uint trianglesNum;
};


groupshared MSSharedData s_sharedData;


struct MeshletTriangle
{
	float4 verticesCS[3]; // vertices in clip space
};


struct TriangleCullingParams
{
	float nearPlane;
	float2 viewportResolution;
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

	// currently we're not handling case when some vertices are in front of near plane, so we just pass those triangles as visible
	if(isInFrontOfPerspectivePlane && false)
	{
		const float2 ca = verticesNDC[1].xy - verticesNDC[0].xy;
		const float2 cb = verticesNDC[2].xy - verticesNDC[0].xy;

		// backface culling
		isTriangleVisible = isTriangleVisible && (ca.x * cb.y >= ca.y * cb.x);
	}
	
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


uint PackVisibilityInfo(uint visibleMeshletIdx, uint triangleIdx)
{
	SPT_CHECK_MSG(triangleIdx < (1 << 7), L"Invalid triangle index - {}", triangleIdx);
	return (visibleMeshletIdx << 7) | triangleIdx;
}


struct PrimitiveOutput
{
	uint packedVisibilityInfo : PACKED_VISBILITY_INFO;
	// TODO - this is not implemented in DXC version used by current vulkan SDK, it seems that there was some on this after vulkan SDK was updated
	// If it's supported in newer version of DXC, we should use it instead of using shared data to compute triangles count and indices
	//bool culled            : SV_CullPrimitive;
};



[outputtopology("triangle")]
[numthreads(MS_GROUP_SIZE, 1, 1)]
void GeometryVisibilityMS(
	in uint3 localID : SV_GroupThreadID,
	in uint3 groupID : SV_GroupID,
	in payload MeshPayload payload,
	out vertices OutputVertex outVerts[MAX_NUM_VERTS],
	out indices uint3 outTriangles[MAX_NUM_PRIMS],
	out primitives PrimitiveOutput outPrims[MAX_NUM_PRIMS]
	)
{
	const MeshletRenderCommand command = MeshletRenderCommand::Decode(payload.commands[groupID.x]);

#if MESHLETS_SHARE_ENTITY
	const uint submeshGlobalIdx = payload.submeshGlobalIdx;
	const uint entityIdx        = payload.entityIdx;
#else
	const uint submeshGlobalIdx = command.submeshGlobalIdx;
	const uint entityIdx        = command.entityIdx;
#endif // MESHLETS_SHARE_ENTITY

	const SubmeshGPUData submesh         = u_submeshes[submeshGlobalIdx];
	const uint globalMeshletIdx          = submesh.meshletsBeginIdx + command.localMeshletIdx;
	const MeshletGPUData meshlet         = u_meshlets[globalMeshletIdx];
	const RenderEntityGPUData entityData = u_renderEntitiesData[entityIdx];

	if (localID.x == 0)
	{
		s_sharedData.trianglesNum = 0;
	}

	const uint primitiveIndicesOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset;
	const uint locationsOffset = submesh.locationsOffset;
	const uint meshletGlobalVertexIndicesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;

	GroupMemoryBarrierWithGroupSync();

	for (uint meshletVertexIdx = localID.x; meshletVertexIdx < meshlet.vertexCount; meshletVertexIdx += MS_GROUP_SIZE)
	{
		const uint vertexIdx = u_geometryData.Load<uint>(meshletGlobalVertexIndicesOffset + (meshletVertexIdx * 4));
		SPT_CHECK_MSG(vertexIdx < submesh.indicesNum, L"Invalid vertex index - {}", vertexIdx);
		const float3 vertexLocation = u_geometryData.Load<float3>(locationsOffset + vertexIdx * 12);
		const float4 vertexView = mul(u_sceneView.viewMatrix, mul(entityData.transform, float4(vertexLocation, 1.f)));
		const float4 vertexCS = mul(u_sceneView.projectionMatrix, vertexView);
		OutputVertex outVert;
		outVert.locationCS = vertexCS;
		s_sharedData.vertices[meshletVertexIdx] = outVert;
	}

	TriangleCullingParams cullingParams;
	cullingParams.nearPlane          = GetNearPlane(u_sceneView);
	cullingParams.viewportResolution = u_viewRenderingParams.renderingResolution;

	GroupMemoryBarrierWithGroupSync();

	for (uint meshletTriangleIdx = localID.x; meshletTriangleIdx < meshlet.triangleCount; meshletTriangleIdx += MS_GROUP_SIZE)
	{
		const uint3 triangleVertices = LoadTriangleVertexIndices(primitiveIndicesOffset, meshletTriangleIdx);

		MeshletTriangle tri;
		[unroll]
		for(uint i = 0; i < 3; i++)
		{
			tri.verticesCS[i] = s_sharedData.vertices[triangleVertices[i]].locationCS;
		}

		const bool isTriangleVisible = IsTriangleVisible(tri, cullingParams);

		if(isTriangleVisible)
		{
			uint outputTriangleIdx = 0;
			const uint2 triangleVisibleBallot = WaveActiveBallot(isTriangleVisible).xy;
			const uint visibleTrianglesNum = countbits(triangleVisibleBallot.x) + countbits(triangleVisibleBallot.y);
			if (WaveIsFirstLane())
			{
				InterlockedAdd(s_sharedData.trianglesNum, visibleTrianglesNum, outputTriangleIdx);
			}
			outputTriangleIdx = WaveReadLaneFirst(outputTriangleIdx) + GetCompactedIndex(triangleVisibleBallot, WaveGetLaneIndex());

			SPT_CHECK_MSG(SPT_SINGLE_ARG(all(triangleVertices < meshlet.vertexCount)), L"Invalid triangle vertices - {}", triangleVertices);
			s_sharedData.triangles[outputTriangleIdx] = PackTriangleVertices(triangleVertices);
		}
	}

	SetMeshOutputCounts(meshlet.vertexCount, s_sharedData.trianglesNum);

	// Write triangles
	for (uint meshletTriangleIdx = localID.x; meshletTriangleIdx < s_sharedData.trianglesNum; meshletTriangleIdx += MS_GROUP_SIZE)
	{
		outTriangles[meshletTriangleIdx] = UnpackTriangleVertices(s_sharedData.triangles[meshletTriangleIdx]);

		PrimitiveOutput primitiveOutput;
		primitiveOutput.packedVisibilityInfo = PackVisibilityInfo(command.visibleMeshletIdx, meshletTriangleIdx);
		outPrims[meshletTriangleIdx] = primitiveOutput;
	}

	// Write vertices
	for (uint meshletVertexIdx = localID.x; meshletVertexIdx < meshlet.vertexCount; meshletVertexIdx += MS_GROUP_SIZE)
	{
		outVerts[meshletVertexIdx] = s_sharedData.vertices[meshletVertexIdx];
	}
}


struct VIS_BUFFER_PS_OUT
{
	uint packedVisibilityInfo : SV_TARGET0;
};


VIS_BUFFER_PS_OUT GeometryVisibilityFS(in OutputVertex vertexInput, in PrimitiveOutput primData)
{
	VIS_BUFFER_PS_OUT output;
	output.packedVisibilityInfo = primData.packedVisibilityInfo;
	return output;
}
