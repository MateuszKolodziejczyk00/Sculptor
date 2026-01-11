#include "SculptorShader.hlsli"

[[descriptor_set(CompilePBRMaterialTexturesDS)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void CompilePBRMaterialTexturesCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	if (u_constants.rwBaseColor.IsValid())
	{
		const float3 color = u_constants.loadedBaseColor.Load(coords).xyz;
		u_constants.rwBaseColor.Store(coords, float4(color, 1.f));
	}

	if (u_constants.rwMetallicRoughness.IsValid())
	{
		const float2 metallicRoughness = u_constants.loadedMetallicRoughness.Load(coords);
		u_constants.rwMetallicRoughness.Store(coords, metallicRoughness);
	}

	if (u_constants.rwNormals.IsValid())
	{
		const float3 normals = u_constants.loadedNormals.Load(coords).xyz;
		u_constants.rwNormals.Store(coords, normals);
	}

	if (u_constants.rwEmissive.IsValid())
	{
		const float3 emissive = u_constants.loadedEmissive.Load(coords).xyz;
		u_constants.rwEmissive.Store(coords, float4(emissive, 1.f));
	}
}
