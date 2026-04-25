#include "SculptorShader.hlsli"


struct VS_INPUT
{
	uint vertexIdx : SV_VertexID;
};


struct VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float2 uv        : UV;
};


VS_OUTPUT FullScreenVS(VS_INPUT input)
{
	const float2 vertexLocations[3] = { float2(-1.f, -1.f), float2(3.f, -1.f), float2(-1.f, 3.f) };
	const float2 uvs[3] = { float2(0.f, 0.f), float2(2.f, 0.f), float2(0.f, 2.f) };

	VS_OUTPUT output;
	output.clipSpace = float4(vertexLocations[input.vertexIdx], 0.5f, 1.f);
	output.uv = uvs[input.vertexIdx];

	return output;
}
