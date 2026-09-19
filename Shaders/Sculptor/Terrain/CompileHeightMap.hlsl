#include "SculptorShader.hlsli"

[[shader_params(CompileHeightMapConstants, PARAMS_COMPILE_HEIGHT_MAP_CONSTANTS)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void CompileHeightMapCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	if (any(coords >= PARAMS_COMPILE_HEIGHT_MAP_CONSTANTS->rwHeightMap.GetResolution()))
	{
		return;
	}

	const float height = PARAMS_COMPILE_HEIGHT_MAP_CONSTANTS->sourceHeightMap.Load(coords).r;
	PARAMS_COMPILE_HEIGHT_MAP_CONSTANTS->rwHeightMap.Store(coords, height);
}
