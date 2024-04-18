#include "SculptorShader.hlsli"

[[descriptor_set(SRCopyRoughnessDS, 0)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void SRCopyRoughnessCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	if(all(pixel < u_constants.resolution))
	{
		const float roughness = u_specularColorAndRoughnessTexture.Load(int3(pixel, 0)).r;
		u_destRoughnessTexture[pixel] = roughness;
	}
}
