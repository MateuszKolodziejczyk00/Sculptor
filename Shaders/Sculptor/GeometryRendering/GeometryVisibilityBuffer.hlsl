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
	//MeshletRenderCommand commands[TS_GROUP_SIZE];
	// looks like we can't use structs in payloads - validation layers are crashing when creating shader module
	// This is MeshletRenderCommand 
	ENCODED_MESHLET_COMMAND_TYPE commands[TS_GROUP_SIZE];
	uint numCommands;

#if MESHLETS_SHARE_ENTITY
	uint batchElementIdx;
#endif // MESHLETS_SHARE_ENTITY
};


groupshared MeshPayload s_payload;


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


struct TSInput
{
	uint3 globalID : SV_DispatchThreadID;
	[[vk::builtin("DrawIndex")]] uint drawCommandIndex : DRAW_INDEX;
};


[numthreads(TS_GROUP_SIZE, 1, 1)]
void GeometryVisibility_TS(in TSInput input)
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
	s_payload.batchElementIdx = batchElementIdx;
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
			visibleMeshletInfo.entityIdx        = batchElement.entityIdx;
			visibleMeshletInfo.meshletGlobalIdx = globalMeshletIdx;
			visibleMeshletInfo.submeshGlobalIdx = batchElement.submeshGlobalIdx;
			visibleMeshletInfo.materialDataID   = batchElement.materialDataID;
			visibleMeshletInfo.materialBatchIdx = batchElement.materialBatchIdx;
			AppendMeshletRenderCommand(visibleMeshletInfo, batchElementIdx, localMeshletIdx);
		}
	}

	DispatchMesh(s_payload.numCommands, 1, 1, s_payload);
}

// This is a bunch of outparams for the Mesh shader. At least SV_Position must be present.
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
	SPT_CHECK_MSG(all(vertices <= maxIndex), "PackTriangleIndices: Invalid triangle vertices - {}", vertices);
	SPT_CHECK_MSG(triangleIdx < maxIndex, "PackTriangleIndices: Invalid triangle idx - {}", triangleIdx)
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


struct MSSharedData
{
	float4 verticesClipSpace[MAX_NUM_VERTS];
	uint   triangles[MAX_NUM_PRIMS];
	uint   trianglesNum;
};


groupshared MSSharedData s_sharedData;
	uint materialDataID : MATERIAL_DATA_ID;


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


struct PrimitiveOutput
{
	uint packedVisibilityInfo : PACKED_VISBILITY_INFO;
	// TODO - this is not implemented in DXC version used by current vulkan SDK, it seems that there was some on this after vulkan SDK was updated
	// If it's supported in newer version of DXC, we should use it instead of using shared data to compute triangles count and indices
	//bool culled            : SV_CullPrimitive;

#if MATERIAL_CAN_DISCARD
	uint16_t materialDataID : MATERIAL_DATA_ID;
#endif // MATERIAL_CAN_DISCARD
};



[outputtopology("triangle")]
[numthreads(MS_GROUP_SIZE, 1, 1)]
void GeometryVisibility_MS(
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
		s_sharedData.verticesClipSpace[meshletVertexIdx] = vertexCS;
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
			tri.verticesCS[i] = s_sharedData.verticesClipSpace[triangleVertices[i]];
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
			s_sharedData.triangles[outputTriangleIdx] = PackTriangleVertices(triangleVertices, meshletTriangleIdx);
		}
	}

	SetMeshOutputCounts(meshlet.vertexCount, s_sharedData.trianglesNum);

	// Write triangles
	for (uint outputTriangleIdx = localID.x; outputTriangleIdx < s_sharedData.trianglesNum; outputTriangleIdx += MS_GROUP_SIZE)
	{
		uint meshletTriangleIdx = 0;
		outTriangles[outputTriangleIdx] = UnpackTriangleVertices(s_sharedData.triangles[outputTriangleIdx], OUT meshletTriangleIdx);

		PrimitiveOutput primitiveOutput;
		primitiveOutput.packedVisibilityInfo = PackVisibilityInfo(command.visibleMeshletIdx, meshletTriangleIdx);
#if MATERIAL_CAN_DISCARD
		primitiveOutput.materialDataID = batchElement.materialDataID;
#endif // MATERIAL_CAN_DISCARD
		outPrims[outputTriangleIdx] = primitiveOutput;
	}

	// Write vertices
	for (uint meshletVertexIdx = localID.x; meshletVertexIdx < meshlet.vertexCount; meshletVertexIdx += MS_GROUP_SIZE)
	{
		OutputVertex outputVertex;
		outputVertex.locationCS = s_sharedData.verticesClipSpace[meshletVertexIdx];
#if MATERIAL_CAN_DISCARD
		const uint vertexIdx = u_geometryData.Load<uint>(meshletGlobalVertexIndicesOffset + (meshletVertexIdx * 4));
		const float2 vertexUV = u_geometryData.Load<float2>(submesh.uvsOffset + vertexIdx * 8);
		outputVertex.uv = vertexUV;
#endif // MATERIAL_CAN_DISCARD
		outVerts[meshletVertexIdx] = outputVertex;
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
    
    const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(primData.materialDataID);

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
