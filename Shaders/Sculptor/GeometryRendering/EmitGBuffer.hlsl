#include "SculptorShader.hlsli"
#include "GeometryRendering/GeometryDefines.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[descriptor_set(EmitGBufferDS)]]

[[descriptor_set(MaterialBatchDS)]]

#define SPT_MATERIAL_SAMPLE_CUSTOM_DERIVATIVES 1

#include "SceneRendering/GPUScene.hlsli"
#include "Utils/Quaternion.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"
#include "Terrain/SceneTerrain.hlsli"

#include "Materials/MaterialSystem.hlsli"


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
	float2 screenUV   : TEXCOORD;
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
	const uint globalVertexIdx    = UGB().LoadVertexIndex(meshletGlobalVertexIndicesOffset, vertexIdx);
	const float3 vertexLocation   = UGB().LoadLocation(submesh.locationsOffset, globalVertexIdx);
	const float3 vertexLocationWS = mul(entityData.transform, float4(vertexLocation, 1.f)).xyz;
	const float4 vertexCS = mul(u_sceneView.viewProjectionMatrix, float4(vertexLocationWS, 1.f));

	VertexData vertexData;
	vertexData.clipSpace = vertexCS;
	vertexData.worldLocation = vertexLocationWS;

	if (submesh.normalsOffset != IDX_NONE_32)
	{
		const float3 vertexNormal = UGB().LoadNormal(submesh.normalsOffset, globalVertexIdx);
		vertexData.normal = normalize(mul(entityData.transform, float4(vertexNormal, 0.f)).xyz);

		hasTangent = hasTangent && submesh.tangentsOffset != IDX_NONE_32;
		if (hasTangent)
		{
			const float4 vertexTangent = UGB().LoadTangent(submesh.tangentsOffset, globalVertexIdx);
			vertexData.tangent   = normalize(mul(entityData.transform, float4(vertexTangent.xyz, 0.f)).xyz);
			vertexData.bitangent = normalize(cross(vertexData.normal, vertexData.tangent)) * vertexTangent.w;
		}
		else
		{
			vertexData.tangent = normalize(dot(vertexData.normal, UP_VECTOR) > 0.9f ? cross(vertexData.normal, RIGHT_VECTOR) : cross(vertexData.normal, UP_VECTOR));
			vertexData.bitangent = normalize(cross(vertexData.normal, vertexData.tangent));
		}
	}

	const float2 normalizedUV = UGB().LoadNormalizedUV(submesh.uvsOffset, globalVertexIdx);
	vertexData.uv = submesh.uvsMin + normalizedUV * submesh.uvsRange;

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
	float2 uvScale;
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
	interpolatedVD.uvScale       = ComputeUVScale(vertices[0].worldLocation, vertices[1].worldLocation, vertices[2].worldLocation, vertices[0].uv, vertices[1].uv, vertices[2].uv);

	return interpolatedVD;
}


InterpolatedVertexData ProcessTriangle(in const GPUVisibleMeshlet visibleMeshlet, in uint triangleIdx, float2 screenPositionClip)
{
	const SubmeshGPUData submesh         = visibleMeshlet.submeshPtr.Load();
	const MeshletGPUData meshlet         = visibleMeshlet.meshletPtr.Load();
	const RenderEntityGPUData entityData = visibleMeshlet.entityPtr.Load();

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

#ifdef DOUBLE_SIDED
	const float det = determinant(float3x3(vertexData[0].clipSpace.xyw, vertexData[1].clipSpace.xyw, vertexData[2].clipSpace.xyw));
	const bool isBackface = det < 0.f;

	if(isBackface)
	{
		interpolatedData.normal    = -interpolatedData.normal;
		interpolatedData.tangent   = -interpolatedData.tangent;
		interpolatedData.bitangent = -interpolatedData.bitangent;
	}
#endif // DOUBLE_SIDED

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


float ApplyParallax(inout MaterialEvaluationParameters evalParams, in MaterialDepthData materialDepthData, in float3 toView, in float2 uvScale)
{
	const float texLevel = 0.f;
	const float heightScale = materialDepthData.maxDepthCm;

	float layersNum = lerp(32.f, 10.f, abs(toView.z)); 
	float layerDepth = 1.f / layersNum;
	float currentLayerDepth = 0.f;

	float2 P = (toView.xy / max(toView.z, 0.05f)) * heightScale * uvScale;
	float2 deltaUV = P / layersNum;

	float2 currentUV = evalParams.uv.uv;
	float currentDepth = materialDepthData.depthTexture.SampleLevel(BindlessSamplers::MaterialAniso(), currentUV, texLevel);
	
	float prevHeight = 0.f;

	while (currentLayerDepth < currentDepth)
	{
		prevHeight = currentDepth;
		currentUV -= deltaUV;
		currentDepth = materialDepthData.depthTexture.SampleLevel(BindlessSamplers::MaterialAniso(), currentUV, texLevel);
		currentLayerDepth += layerDepth;
	}

	float2 prevUV = currentUV + deltaUV;
	float nextDepth = currentDepth - currentLayerDepth;
	float prevDepth = prevHeight - (currentLayerDepth - layerDepth);

	float weight = nextDepth / (nextDepth - prevDepth);
	evalParams.uv.uv = lerp(currentUV, prevUV, weight);

	const float finalDepth = lerp(currentLayerDepth, currentLayerDepth - layerDepth, weight) * heightScale;
	const float dist = finalDepth / abs(toView.z);
	return dist;
}


struct FS_Output
{
	float4 gBuffer0  : SV_TARGET0;
	float4 gBuffer1  : SV_TARGET1;
	float  gBuffer2  : SV_TARGET2;
	float3 gBuffer3  : SV_TARGET3;
	uint   gBuffer4  : SV_TARGET4;
	float  occlusion : SV_TARGET5;
#if ENABLE_POM
	float  pomDepth  : SV_TARGET6;
#endif // ENABLE_POM
};


FS_Output EmitGBuffer_FS(in OutputVertex vertexInput)
{
	FS_Output output = (FS_Output )0;

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

		MaterialEvaluationParameters materialEvalParams = CreateMaterialEvalParams(vertexData);

		const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(visibleMeshlet.materialDataHandle);

#if ENABLE_POM && defined(MATERIAL_DEPTH_DATA_ACCESSOR)
		MaterialDepthData materialDepthData = materialData.MATERIAL_DEPTH_DATA_ACCESSOR;
		float pomDepth = 0.f;
		if (materialDepthData.depthTexture.IsValid())
		{
			const float3x3 tbn = float3x3(vertexData.tangent, -vertexData.bitangent, vertexData.normal);
			const float3 toViewWS = normalize(u_sceneView.viewLocation - vertexData.worldLocation);
			const float3 toViewTS = normalize(mul(tbn, toViewWS));
			pomDepth = ApplyParallax(materialEvalParams, materialDepthData, toViewTS, vertexData.uvScale);
			pomDepth *=  100.f / POM_MAX_DEPTH_OFFSET_CM;
			pomDepth *= abs(dot(u_sceneView.viewForward, toViewWS));
		}
#endif // ENABLE_POM && defined(MATERIAL_DEPTH_DATA_ACCESSOR)

		const MaterialEvaluationOutput evaluatedMaterial = EvaluateMaterial(materialEvalParams, materialData);

		GBufferData gBufferData;
		gBufferData.baseColor = evaluatedMaterial.baseColor;
		gBufferData.metallic  = evaluatedMaterial.metallic;
		gBufferData.normal    = evaluatedMaterial.shadingNormal;
		gBufferData.tangent   = vertexData.tangent;
		gBufferData.bitangent = vertexData.bitangent;
		gBufferData.roughness = evaluatedMaterial.roughness;
		if (gBufferData.roughness == 0.f)
		{
			gBufferData.roughness = 0.6f;
		}
		gBufferData.emissive  = evaluatedMaterial.emissiveColor;

		const GBufferOutput gBufferOutput = EncodeGBuffer(gBufferData);

		output.gBuffer0  = gBufferOutput.gBuffer0;
		output.gBuffer1  = gBufferOutput.gBuffer1;
		output.gBuffer2  = gBufferOutput.gBuffer2;
		output.gBuffer3  = gBufferOutput.gBuffer3;
		output.gBuffer4  = gBufferOutput.gBuffer4;
		output.occlusion = evaluatedMaterial.occlusion;
#if ENABLE_POM
		output.pomDepth  = pomDepth;
#endif // ENABLE_POM
	}

	return output;
}


FS_Output EmitTerrainGBuffer_FS(in OutputVertex vertexInput)
{
	const uint2  pixelCoord = u_emitGBufferConstants.screenResolution * vertexInput.screenUV;
	const float  depth      = u_depthTexture.Load(uint3(pixelCoord, 0u)).x;
	const float3 ndc        = float3(vertexInput.screenUV * 2.f - 1.f, depth);

	const float3 locationWS = NDCToWorldSpace(ndc, u_sceneView);

	const TerrainInterface terrain = SceneTerrain();

	const float3 terrainNormal    = terrain.GetNormal(locationWS.xy);
	const float3 terrainTangent   = terrain.GetTangent(locationWS.xy);
	const float3 terrainBitangent = terrain.GetBitangent(locationWS.xy);

	GBufferData gBufferData;
	gBufferData.baseColor = lerp(float3(0.1f, 0.3f, 0.1f), float3(0.4f, 0.6f, 0.4f), saturate(locationWS.z * 0.03f));
	gBufferData.metallic  = 0.f;
	gBufferData.normal    = terrainNormal;
	gBufferData.tangent   = terrainTangent;
	gBufferData.bitangent = terrainBitangent;
	gBufferData.roughness = 0.88f;
	gBufferData.emissive  = 0.f;

	const GBufferOutput gBufferOutput = EncodeGBuffer(gBufferData);

	FS_Output output;
	output.gBuffer0  = gBufferOutput.gBuffer0;
	output.gBuffer1  = gBufferOutput.gBuffer1;
	output.gBuffer2  = gBufferOutput.gBuffer2;
	output.gBuffer3  = gBufferOutput.gBuffer3;
	output.gBuffer4  = gBufferOutput.gBuffer4;
	output.occlusion = 1.f;
#if ENABLE_POM
	output.pomDepth  = 0.f;
#endif // ENABLE_POM

	return output;
}
