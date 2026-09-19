#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(MaterialDepthParams, PARAMS_CREATE_MATERIAL_DEPTH)]]

#include "GeometryRendering/GeometryCommon.hlsli"


struct VS_INPUT
{
	uint index : SV_VertexID;
};


struct VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float2 uv : UV;
};


VS_OUTPUT MaterialDepthVS(VS_INPUT input)
{
	const float2 vertexLocations[3] = { float2(-1.f, -1.f), float2(3.f, -1.f), float2(-1.f, 3.f) };
	const float2 uvs[3] = { float2(0.f, 0.f), float2(2.f, 0.f), float2(0.f, 2.f) };

	VS_OUTPUT output;
	output.clipSpace = float4(vertexLocations[input.index], 0.f, 1.f);
	output.uv = uvs[input.index];

	return output;
}


struct MATERIAL_DEPTH_OUTPUT
{
	float materialDepth : SV_DEPTH0;
};


MATERIAL_DEPTH_OUTPUT MaterialDepthFS(VS_OUTPUT vertexInput)
{
	const uint2 pixelCoord = PARAMS_CREATE_MATERIAL_DEPTH->screenResolution * vertexInput.uv;

	const uint packedVisibilityInfo = PARAMS_CREATE_MATERIAL_DEPTH->visibilityTexture.Load(uint3(pixelCoord, 0u));

	uint visibleMeshletIdx = 0;
	uint visibleTriangleIdx = 0;
	const uint primType = UnpackVisibilityInfo(packedVisibilityInfo, OUT visibleMeshletIdx, OUT visibleTriangleIdx);

	uint batchIdx = ~0u;
	if (primType == VISIBLE_PRIMITIVE_TYPE_GEOMETRY)
	{
		const GPUVisibleMeshlet visibleMeshlet = PARAMS_CREATE_MATERIAL_DEPTH->visibleMeshlets[visibleMeshletIdx];
		batchIdx = visibleMeshlet.materialBatchIdx;
	}
	else if (primType == VISIBLE_PRIMITIVE_TYPE_TERRAIN)
	{
		batchIdx = PARAMS_CREATE_MATERIAL_DEPTH->terrainMaterialBatchIdx;
	}
	else if (primType == VISIBLE_PRIMITIVE_TYPE_GRASS)
	{
		batchIdx = PARAMS_CREATE_MATERIAL_DEPTH->grassMaterialBatchIdx;
	}

	const float materialDepth = batchIdx != ~0u ? MaterialBatchIdxToMaterialDepth(batchIdx) : 1.f;

	MATERIAL_DEPTH_OUTPUT output;
	output.materialDepth = materialDepth;
	return output;
}
