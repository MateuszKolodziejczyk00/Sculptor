#include "SculptorShader.hlsli"

[[shader_params(CompilePBRMaterialConstants, u_constants)]]

#include "Utils/Packing.hlsli"


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
		const float4 color = u_constants.loadedBaseColor.Load(coords);
		u_constants.rwBaseColor.Store(coords, float4(color.rgb, 1.0f));

		if (u_constants.rwAlpha.IsValid())
		{
			u_constants.rwAlpha.Store(coords, color.a);
		}
	}

	if (u_constants.rwMetallicRoughness.IsValid())
	{
		const float2 metallicRoughness = u_constants.loadedMetallicRoughness.Load(coords);
		u_constants.rwMetallicRoughness.Store(coords, float4(metallicRoughness, 0.0f, 0.f));
	}

	if (u_constants.rwNormals.IsValid())
	{
		float3 normal = u_constants.loadedNormals.Load(coords).xyz;
		normal.xy = normal.xy * 2.f - 1.f;
		u_constants.rwNormals.Store(coords, PackTangentNormalToXY(normal));
	}

	if (u_constants.rwEmissive.IsValid())
	{
		const float3 emissive = u_constants.loadedEmissive.Load(coords).xyz;
		u_constants.rwEmissive.Store(coords, float4(emissive, 1.f));
	}
}
