#include "SculptorShader.hlsli"

[[shader_params(CompilePBRMaterialConstants, PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS)]]

#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void CompilePBRMaterialTexturesCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwBaseColor.IsValid())
	{
		const float4 color = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedBaseColor.Load(coords);
		PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwBaseColor.Store(coords, float4(color.rgb, 1.0f));

		if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwAlpha.IsValid())
		{
			PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwAlpha.Store(coords, color.a);
		}
	}

	if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwMetallicRoughness.IsValid())
	{
		float2 metallicRoughness = 1.f;
		if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedMetallicRoughness.IsValid())
		{
			metallicRoughness = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedMetallicRoughness.Load(coords);
		}
		else if ( PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedRoughness.IsValid())
		{
			metallicRoughness.y = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedRoughness.Load(coords);
		}

		PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwMetallicRoughness.Store(coords, float4(metallicRoughness, 0.0f, 0.f));
	}

	if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwNormals.IsValid())
	{
		float3 normal = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedNormals.Load(coords).xyz;
		normal.xy = normal.xy * 2.f - 1.f;
		PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwNormals.Store(coords, PackTangentNormalToXY(normal));
	}

	if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwEmissive.IsValid())
	{
		const float3 emissive = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedEmissive.Load(coords).xyz;
		PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwEmissive.Store(coords, float4(emissive, 1.f));
	}

	if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwDepth.IsValid())
	{
		const float depth = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedDepth.Load(coords);
		PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwDepth.Store(coords, depth);
	}

	if (PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwDisplacement.IsValid())
	{
		const float displacement = PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->loadedDisplacement.Load(coords) * PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->displacementScale;
		PARAMS_COMPILE_P_B_R_MATERIAL_CONSTANTS->rwDisplacement.Store(coords, displacement);
	}
}
