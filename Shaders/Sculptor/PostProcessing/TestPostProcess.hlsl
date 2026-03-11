#include "SculptorShader.hlsli"

[[shader_params(TestConstants, u_constants)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void TestPostProcessCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	const uint2 resolution = u_constants.tex.GetResolution();

	const uint2 in_coords = (coords + uint2(u_constants.time * 20.f, 0)) % resolution;

	//const float3 value = u_constants.tex.Load(coords).rgb + u_constants.color;
	const float3 value = u_constants.invert ? 1.f - sqrt(u_constants.tex.Load(coords).rgb) : sqrt(u_constants.tex.Load(coords).rgb);

	u_constants.tex.Store(coords, float4(value, 1.f));
}
