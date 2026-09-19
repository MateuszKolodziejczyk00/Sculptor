#include "SculptorShader.hlsli"

[[shader_params(TestConstants, PARAMS_TEST_CONSTANTS)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void TestPostProcessCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	const uint2 resolution = PARAMS_TEST_CONSTANTS->tex.GetResolution();

	const uint2 in_coords = (coords + uint2(PARAMS_TEST_CONSTANTS->time * 20.f, 0)) % resolution;

	//const float3 value = PARAMS_TEST_CONSTANTS->tex.Load(coords).rgb + PARAMS_TEST_CONSTANTS->color;
	const float3 value = PARAMS_TEST_CONSTANTS->invert ? 1.f - sqrt(PARAMS_TEST_CONSTANTS->tex.Load(coords).rgb) : sqrt(PARAMS_TEST_CONSTANTS->tex.Load(coords).rgb);

	PARAMS_TEST_CONSTANTS->tex.Store(coords, float4(value, 1.f));
}
