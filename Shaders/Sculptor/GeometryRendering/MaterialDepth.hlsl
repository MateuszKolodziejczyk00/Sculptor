#include "SculptorShader.hlsli"

[[descriptor_set(CreateMaterialDepthDS, 0)]]

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
	float materialDepth = 1.f;

	const uint2 pixelCoord = u_materialDepthParams.screenResolution * vertexInput.uv;

	const uint packedVisibilityInfo = u_visibilityTexture.Load(uint3(pixelCoord, 0u));

	uint visibleMeshletIdx = 0;
	uint visibleTriangleIdx = 0;
	const bool isValidGeometry = UnpackVisibilityInfo(packedVisibilityInfo, OUT visibleMeshletIdx, OUT visibleTriangleIdx);

	if (isValidGeometry)
	{
		const GPUVisibleMeshlet visibleMeshlet = u_visibleMeshlets[visibleMeshletIdx];
		materialDepth = MaterialBatchIdxToMaterialDepth(visibleMeshlet.materialBatchIdx);
	}

	MATERIAL_DEPTH_OUTPUT output;
	output.materialDepth = materialDepth;
	return output;
}
