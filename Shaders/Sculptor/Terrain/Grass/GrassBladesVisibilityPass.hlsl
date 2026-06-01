#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(GrassBladesVisibilityPassDS)]]

#include "GeometryRendering/GeometryCommon.hlsli"
#include "Terrain/Grass/GrassGeometry.hlsli"


#define GRASS_VISIBILITY_MS_GROUP_SIZE 64u
#define GRASS_BLADES_PER_GROUP         9u


struct MeshVertex
{
	float4 clipSpace : SV_Position;
};


struct PrimitiveData
{
	uint packedVisibilityInfo : PACKED_VISBILITY_INFO;
};


struct MeshShaderInput
{
	uint3 meshGroupID : SV_GroupID;
	uint localID      : SV_GroupIndex;
};


[outputtopology("triangle")]
[numthreads(GRASS_VISIBILITY_MS_GROUP_SIZE, 1, 1)]
void GrassBladesVisibilityPassMS(in MeshShaderInput input,
								 out vertices MeshVertex outVertices[GRASS_BLADES_PER_GROUP * GRASS_BLADE_VERTICES_NUM],
								 out indices uint3 outTriangles[GRASS_BLADES_PER_GROUP * GRASS_BLADE_TRIANGLES_NUM],
								 out primitives PrimitiveData outPrimitives[GRASS_BLADES_PER_GROUP * GRASS_BLADE_TRIANGLES_NUM])
{
	const uint bladesNum      = u_constants.bladesNum.Load(0u);
	const uint firstBladeIdx  = input.meshGroupID.x * GRASS_BLADES_PER_GROUP;
	const uint bladesInGroup  = firstBladeIdx < bladesNum ? min(GRASS_BLADES_PER_GROUP, bladesNum - firstBladeIdx) : 0u;
	const uint verticesNum    = bladesInGroup * GRASS_BLADE_VERTICES_NUM;
	const uint trianglesNum   = bladesInGroup * GRASS_BLADE_TRIANGLES_NUM;

	SetMeshOutputCounts(verticesNum, trianglesNum);

	for (uint vertexIdx = input.localID; vertexIdx < verticesNum; vertexIdx += GRASS_VISIBILITY_MS_GROUP_SIZE)
	{
		const uint bladeLocalIdx       = vertexIdx / GRASS_BLADE_VERTICES_NUM;
		const uint bladeVertexIdx      = vertexIdx % GRASS_BLADE_VERTICES_NUM;
		const uint bladeIdx            = firstBladeIdx + bladeLocalIdx;
		const GrassBladeDef bladeDef   = u_constants.bladeDefs.Load(bladeIdx);
		GrassVertexProcessor vertexProcessor = GrassVertexProcessor::Create(bladeDef);
		const float3 vertexLocationWS  = vertexProcessor.GetVertexLocation(bladeVertexIdx);

		outVertices[vertexIdx].clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(vertexLocationWS, 1.f));
	}

	for (uint triangleIdx = input.localID; triangleIdx < trianglesNum; triangleIdx += GRASS_VISIBILITY_MS_GROUP_SIZE)
	{
		const uint bladeLocalIdx         = triangleIdx / GRASS_BLADE_TRIANGLES_NUM;
		const uint bladeTriangleIdx      = triangleIdx % GRASS_BLADE_TRIANGLES_NUM;
		const uint bladeIdx              = firstBladeIdx + bladeLocalIdx;
		const uint vertexOffset          = bladeLocalIdx * GRASS_BLADE_VERTICES_NUM;

		outTriangles[triangleIdx] = LoadGrassBladeTriangleIndices(bladeTriangleIdx) + vertexOffset;
		outPrimitives[triangleIdx].packedVisibilityInfo = PackGrassVisibilityInfo(bladeIdx, bladeTriangleIdx);
	}
}


struct FS_VIS_OUTPUT
{
	uint visibility : SV_TARGET0;
};


FS_VIS_OUTPUT GrassBladesVisibilityPassFS(in PrimitiveData primData)
{
	FS_VIS_OUTPUT output;
	output.visibility = primData.packedVisibilityInfo;
	return output;
}
