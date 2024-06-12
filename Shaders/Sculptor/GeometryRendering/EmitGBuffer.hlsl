#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryDefines.hlsli"

[[descriptor_set(StaticMeshUnifiedDataDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

[[descriptor_set(GeometryDS, 3)]]

[[descriptor_set(MaterialsDS, 4)]]

[[descriptor_set(EmitGBufferDS, 5)]]

[[descriptor_set(MaterialBatchDS, 6)]]


#include "Utils/Quaternion.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/GBuffer.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"

#define SPT_MATERIAL_SAMPLE_CUSTOM_DERIVATIVES 1

#include "Materials/MaterialSystem.hlsli"
#include SPT_MATERIAL_SHADER_PATH


#define MS_GROUP_SIZE_X 8
#define MS_GROUP_SIZE_Y 4
#define MS_GROUP_SIZE 32

// We output quads (2 triangles) in 8 x 4 blocks
//  This gives us 64 triangles and 45 vertices
#define MAX_NUM_PRIMS 64
#define MAX_NUM_VERTS 45


struct OutputVertex
{
	float4 locationCS : SV_Position;
	float2 screenUV : TEXCOORD;
};


uint LocalVertexLocationToIndex(uint2 localLocation)
{
	return localLocation.x + localLocation.y * 9;
}


uint2 IndexToLocalVertexLocation(uint index)
{
	return uint2(index % 9, index / 9);
}


[outputtopology("triangle")]
[numthreads(MS_GROUP_SIZE, 1, 1)]
void EmitGBuffer_MS(
	in uint3 localID : SV_GroupThreadID,
	in uint3 groupID : SV_GroupID,
	out vertices OutputVertex outVerts[MAX_NUM_VERTS],
	out indices uint3 outTriangles[MAX_NUM_PRIMS]
	)
{
	const uint2 groupLocationOnGrid = uint2(groupID.x % u_emitGBufferConstants.groupsPerRow, groupID.x / u_emitGBufferConstants.groupsPerRow);
	const uint2 globalTileID = groupLocationOnGrid * uint2(MS_GROUP_SIZE_X, MS_GROUP_SIZE_Y) + uint2(localID.x % MS_GROUP_SIZE_X, localID.x / MS_GROUP_SIZE_X);

	const uint2 materialDepthTile = u_materialDepthTilesTexture.Load(uint3(globalTileID, 0u)).xy;
	const uint materialBatchIdx = u_materialBatchConstants.materialBatchIdx;

	const bool tileContainsMaterial = materialDepthTile.x <= materialBatchIdx && materialBatchIdx <= materialDepthTile.y;

	const uint2 tileContainsMaterialBallot = WaveActiveBallot(tileContainsMaterial).xy;
	const uint tilesContainingMaterialNum = countbits(tileContainsMaterialBallot.x) + countbits(tileContainsMaterialBallot.y);

	const uint outputTileIdx = GetCompactedIndex(tileContainsMaterialBallot, WaveGetLaneIndex());

	const uint trianglesNum = tilesContainingMaterialNum * 2;

	SetMeshOutputCounts(MAX_NUM_VERTS, trianglesNum);

	const float2 groupNDCLocation = -1.f + groupLocationOnGrid * float2(MS_GROUP_SIZE_X, MS_GROUP_SIZE_Y) * u_emitGBufferConstants.tileSizeNDC;

	for (uint vertexIdx = localID.x; vertexIdx < MAX_NUM_VERTS; vertexIdx += MS_GROUP_SIZE)
	{
		const uint2 vertexLocation = IndexToLocalVertexLocation(vertexIdx);

		const float3 vertexPositionNDC = float3(groupNDCLocation + vertexLocation * u_emitGBufferConstants.tileSizeNDC, u_materialBatchConstants.materialBatchDepth);
		OutputVertex vertex;
		vertex.locationCS = float4(vertexPositionNDC, 1.0f);
		vertex.screenUV   = (vertexPositionNDC.xy + 1.f) * 0.5f;

		outVerts[vertexIdx] = vertex;
	}

	if (tileContainsMaterial)
	{
		const uint localTileIdx = localID.x;
		const uint outputTriangleIdx = outputTileIdx * 2;

		const uint2 localTileOffset = uint2(localTileIdx % 8, localTileIdx / 8);
		
		outTriangles[outputTriangleIdx][0] = LocalVertexLocationToIndex(localTileOffset);
		outTriangles[outputTriangleIdx][1] = LocalVertexLocationToIndex(localTileOffset + uint2(1, 0));
		outTriangles[outputTriangleIdx][2] = LocalVertexLocationToIndex(localTileOffset + uint2(0, 1));

		outTriangles[outputTriangleIdx + 1][0] = LocalVertexLocationToIndex(localTileOffset + uint2(1, 0));
		outTriangles[outputTriangleIdx + 1][1] = LocalVertexLocationToIndex(localTileOffset + uint2(0, 1));
		outTriangles[outputTriangleIdx + 1][2] = LocalVertexLocationToIndex(localTileOffset + uint2(1, 1));
	}
}


struct VertexData
{
	float3 worldLocation;
	float4 clipSpace;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float2 uv;
};


VertexData ProcessVertex(in RenderEntityGPUData entityData, in const SubmeshGPUData submesh, in const MeshletGPUData meshlet, in uint vertexIdx, inout bool hasTangent)
{
	const uint meshletGlobalVertexIndicesOffset = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;
	const uint globalVertexIdx = u_geometryData.Load<uint>(meshletGlobalVertexIndicesOffset + (vertexIdx * 4));
	const float3 vertexLocation = u_geometryData.Load<float3>(submesh.locationsOffset + globalVertexIdx * 12);
	const float4 vertexView = mul(u_sceneView.viewMatrix, mul(entityData.transform, float4(vertexLocation, 1.f)));
	const float4 vertexCS = mul(u_sceneView.projectionMatrix, vertexView);

	VertexData vertexData;
	vertexData.worldLocation = vertexLocation;
	vertexData.clipSpace     = vertexCS;

	if (submesh.normalsOffset != IDX_NONE_32)
	{
		const float3 vertexNormal = u_geometryData.Load<float3>(submesh.normalsOffset + globalVertexIdx * 12);
		vertexData.normal = normalize(mul(entityData.transform, float4(vertexNormal, 0.f)).xyz);

		hasTangent = hasTangent && submesh.tangentsOffset != IDX_NONE_32;
		if (hasTangent)
		{
			const float4 vertexTangent = u_geometryData.Load<float4>(submesh.tangentsOffset + globalVertexIdx * 16);
			vertexData.tangent   = normalize(mul(entityData.transform, float4(vertexTangent.xyz, 0.f)).xyz);
			vertexData.bitangent = normalize(cross(vertexData.normal, vertexData.tangent)) * vertexTangent.w;
		}
		else
		{
			vertexData.tangent = normalize(dot(vertexData.normal, UP_VECTOR) > 0.9f ? cross(vertexData.normal, RIGHT_VECTOR) : cross(vertexData.normal, UP_VECTOR));
			vertexData.bitangent = normalize(cross(vertexData.normal, vertexData.tangent));
		}
	}

	vertexData.uv = u_geometryData.Load<float2>(submesh.uvsOffset + globalVertexIdx * 8);

	return vertexData;
}


struct InterpolatedVertexData
{
	float3 worldLocation;
	float4 clipSpace;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	bool hasTangent;
	TextureCoord uv;
};


InterpolatedVertexData Interpolate(in VertexData vertices[3], in Barycentrics barycentrics, bool hasTangent)
{
	InterpolatedVertexData interpolatedVD;

	interpolatedVD.normal        = normalize(InterpolateAttribute<float3>(vertices[0].normal, vertices[1].normal, vertices[2].normal, barycentrics));
	interpolatedVD.tangent       = normalize(InterpolateAttribute<float3>(vertices[0].tangent, vertices[1].tangent, vertices[2].tangent, barycentrics));
	interpolatedVD.bitangent     = normalize(InterpolateAttribute<float3>(vertices[0].bitangent, vertices[1].bitangent, vertices[2].bitangent, barycentrics));
	interpolatedVD.hasTangent    = hasTangent;
	interpolatedVD.uv            = InterpolateTextureCoord(vertices[0].uv, vertices[1].uv, vertices[2].uv, barycentrics);
	interpolatedVD.worldLocation = InterpolateAttribute<float3>(vertices[0].worldLocation, vertices[1].worldLocation, vertices[2].worldLocation, barycentrics);
	interpolatedVD.clipSpace     = InterpolateAttribute<float4>(vertices[0].clipSpace, vertices[1].clipSpace, vertices[2].clipSpace, barycentrics);

	return interpolatedVD;
}


InterpolatedVertexData ProcessTriangle(in const GPUVisibleMeshlet visibleMeshlet, in uint triangleIdx, float2 screenPositionClip)
{
	const SubmeshGPUData submesh         = u_submeshes[visibleMeshlet.submeshGlobalIdx];
	const MeshletGPUData meshlet         = u_meshlets[visibleMeshlet.meshletGlobalIdx];
	const RenderEntityGPUData entityData = u_renderEntitiesData[visibleMeshlet.entityIdx];

	const uint primitiveIndicesOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset;

	const uint3 vertexIndices = LoadTriangleVertexIndices(primitiveIndicesOffset, triangleIdx);

	bool hasTangent = true;

	VertexData vertexData[3];
	for(uint triangleVertexIdx = 0u; triangleVertexIdx < 3u; ++triangleVertexIdx)
	{
		vertexData[triangleVertexIdx] = ProcessVertex(entityData, submesh, meshlet, vertexIndices[triangleVertexIdx], INOUT hasTangent);
	}

	const Barycentrics barycentrics = ComputeTriangleBarycentrics(screenPositionClip, vertexData[0].clipSpace, vertexData[1].clipSpace, vertexData[2].clipSpace, u_emitGBufferConstants.invScreenResolution);

	InterpolatedVertexData interpolatedData = Interpolate(vertexData, barycentrics, hasTangent);

#ifdef SPT_MATERIAL_DOUBLE_SIDED
	const float det = determinant(float3x3(vertexData[0].clipSpace.xyw, vertexData[1].clipSpace.xyw, vertexData[2].clipSpace.xyw));
	const bool isBackface = det < 0.f;

	if(isBackface)
	{
		interpolatedData.normal    = -interpolatedData.normal;
		interpolatedData.tangent   = -interpolatedData.tangent;
		interpolatedData.bitangent = -interpolatedData.bitangent;
	}
#endif // SPT_MATERIAL_DOUBLE_SIDED

	return interpolatedData;
}


MaterialEvaluationParameters CreateMaterialEvalParams(in InterpolatedVertexData vertexData)
{
	MaterialEvaluationParameters materialEvalParams;

	materialEvalParams.normal        = vertexData.normal;
	materialEvalParams.tangent       = vertexData.tangent;
	materialEvalParams.bitangent     = vertexData.bitangent;
	materialEvalParams.hasTangent    = vertexData.hasTangent;
	materialEvalParams.uv            = vertexData.uv;
	materialEvalParams.worldLocation = vertexData.worldLocation;
	materialEvalParams.clipSpace     = vertexData.clipSpace;

	return materialEvalParams;
}


GBufferOutput EmitGBuffer_FS(in OutputVertex vertexInput)
{
	GBufferOutput output = (GBufferOutput)0;

	const uint2 pixelCoord = u_emitGBufferConstants.screenResolution * vertexInput.screenUV;
	
	const uint packedVisibilityInfo = u_visibilityTexture.Load(uint3(pixelCoord, 0u));

	uint visibleMeshletIdx = 0;
	uint visibleTriangleIdx = 0;
	const bool isValidGeometry = UnpackVisibilityInfo(packedVisibilityInfo, OUT visibleMeshletIdx, OUT visibleTriangleIdx);

	if (isValidGeometry)
	{
		const GPUVisibleMeshlet visibleMeshlet = u_visibleMeshlets[visibleMeshletIdx];
		
		const float2 screenPositionClip = vertexInput.screenUV * 2.f - 1.f;
		const InterpolatedVertexData vertexData = ProcessTriangle(visibleMeshlet, visibleTriangleIdx, screenPositionClip);

		const MaterialEvaluationParameters materialEvalParams = CreateMaterialEvalParams(vertexData);

		const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(visibleMeshlet.materialDataID);
		const MaterialEvaluationOutput evaluatedMaterial = EvaluateMaterial(materialEvalParams, materialData);

		GBufferData gBufferData;
		gBufferData.baseColor = evaluatedMaterial.baseColor;
		gBufferData.metallic  = evaluatedMaterial.metallic;
		//gBufferData.metallic  = 1.f;
		gBufferData.normal    = evaluatedMaterial.shadingNormal;
		gBufferData.tangent   = vertexData.tangent;
		gBufferData.bitangent = vertexData.bitangent;
		gBufferData.roughness = evaluatedMaterial.roughness;
		//gBufferData.roughness = 0.1f;
		gBufferData.emissive  = evaluatedMaterial.emissiveColor;

		output = EncodeGBuffer(gBufferData);
	}

	return output;
}
