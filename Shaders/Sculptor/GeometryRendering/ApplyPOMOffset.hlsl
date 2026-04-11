#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[descriptor_set(ApplyPOMOffsetDS)]]

#include "Utils/SceneViewUtils.hlsli"


struct VS_INPUT
{
	uint vertexIdx : SV_VertexID;
};


struct VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float2 uv        : UV;
};


VS_OUTPUT ApplyPOMOffsetVS(VS_INPUT input)
{
	const float2 vertexLocations[3] = { float2(-1.f, -1.f), float2(3.f, -1.f), float2(-1.f, 3.f) };
	const float2 uvs[3] = { float2(0.f, 0.f), float2(2.f, 0.f), float2(0.f, 2.f) };

	VS_OUTPUT output;
	output.clipSpace = float4(vertexLocations[input.vertexIdx], 0.5f, 1.f);
	output.uv = uvs[input.vertexIdx];

	return output;
}


struct PS_OUTPUT
{
	float depth : SV_DepthLessEqual;
};


PS_OUTPUT ApplyPOMOffsetFS(VS_OUTPUT input)
{
	const int2 coords = input.uv * u_constants.resolution;

	const float depth = u_constants.depth.Load(coords);
	const float linearDepth = ComputeLinearDepth(depth, u_sceneView);

	const float pomDepth = u_constants.pomDepth.Load(coords) * 0.01f * POM_MAX_DEPTH_OFFSET_CM;

	const float finalDepth = linearDepth + pomDepth;

	PS_OUTPUT output;
	output.depth = GetNearPlane(u_sceneView) / finalDepth;

	return output;
}
