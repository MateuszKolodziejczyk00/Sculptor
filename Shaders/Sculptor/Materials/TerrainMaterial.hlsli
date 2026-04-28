#ifndef TERRAIN_MATERIAL_HLSLI
#define TERRAIN_MATERIAL_HLSLI

#include "SceneRendering/GPUScene.hlsli"
#include "Utils/Packing.hlsli"
#include "Terrain/SceneTerrain.hlsli"


CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    CustomOpacityOutput output;
    output.shouldDiscard = false;
    return output;
}


MaterialEvaluationOutput EvaluateMaterial(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	MaterialEvaluationOutput output;

	TerrainInterface terrainInterface = SceneTerrain();

	float3 baseColor;
	float3 normal;
	float  metallic;
	float  roughness;
	float  occlusion;

	bool foundCachedMaterial = false;
	for (int i = 0; i < 6; ++i)
	{
		TerrrainMaterialCacheLOD matCacheLOD = terrainInterface.materialCache.lods[i];
		const float2 uv = (evalParams.worldLocation.xy - matCacheLOD.minBounds) * matCacheLOD.rcpRange;

		if (all(saturate(uv) == uv))
		{
			const float4 baseColorMetallic  = matCacheLOD.baseColorMetallic.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0);
			const float2 octNormals         = matCacheLOD.normals.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0);
			const float2 roughnessOcclusion = matCacheLOD.roughnessOcclusion.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0);

			baseColor = baseColorMetallic.xyz;
			metallic  = baseColorMetallic.w;
			normal    = OctahedronDecodeNormal(octNormals);
			roughness = roughnessOcclusion.x;
			occlusion = roughnessOcclusion.y;

			foundCachedMaterial = true;

			break;
		}
	}

	if (!foundCachedMaterial)
	{
		const float2 farLODUV = (evalParams.worldLocation.xy + 1024.f) / 2048.f; // This should match the UV calculation in BakeFarLODCS

		const float3 farLODBaseColor = terrainInterface.farLODBaseColor.SampleLevel(BindlessSamplers::LinearClampEdge(), farLODUV, 0);
		const float3 farLODProps     = terrainInterface.farLODProps.SampleLevel(BindlessSamplers::LinearClampEdge(), farLODUV, 0);

		baseColor = farLODBaseColor;
		normal    = terrainInterface.GetNormal(evalParams.worldLocation.xy);
		roughness = farLODProps.x;
		metallic  = farLODProps.y;
		occlusion = farLODProps.z;
	}

	output.shadingNormal  = normal;
	output.geometryNormal = normal;
	output.roughness      = roughness;
	output.baseColor      = baseColor;
	output.metallic       = metallic;
	output.emissiveColor  = 0.f;
	output.occlusion      = occlusion;

	return output;
}

#endif // TERRAIN_MATERIAL_HLSLI
