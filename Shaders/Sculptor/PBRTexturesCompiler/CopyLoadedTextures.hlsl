#include "SculptorShader.hlsli"

[[descriptor_set(CopyToPBRTexturesDS, 0)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void CopyCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	u_rwBaseColor[pixel]         = u_loadedBaseColor.Load(uint3(pixel, 0));
	u_rwMetallicRoughness[pixel] = u_loadedMetallicRoughness.Load(uint3(pixel, 0)).xy;
	u_rwNormals[pixel]           = u_loadedNormals.Load(uint3(pixel, 0));
}
