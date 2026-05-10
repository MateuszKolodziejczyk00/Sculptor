#include "SculptorShader.hlsli"

[[shader_params(CompileHeightMapConstants, u_constants)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void CompileHeightMapCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	if (any(coords >= u_constants.rwHeightMap.GetResolution()))
	{
		return;
	}

	const float height = u_constants.sourceHeightMap.Load(coords).r;
	u_constants.rwHeightMap.Store(coords, height);
}
