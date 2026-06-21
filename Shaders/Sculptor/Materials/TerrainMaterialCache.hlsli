#ifndef TERRAIN_MATERIAL_HLSLI
#define TERRAIN_MATERIAL_HLSLI

#include "SceneRendering/GPUScene.hlsli"
#include "Utils/Packing.hlsli"
#include "Terrain/SceneTerrain.hlsli"

#define TERRAIN_MATERIAL


[[shader_struct(MaterialDepthData)]]


#define TERRAIN_POM_LODS_NUM 5


template<typename TSampler>
CustomOpacityOutput EvaluateCustomOpacity(TSampler sampler, MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    CustomOpacityOutput output;
    output.shouldDiscard = false;
    return output;
}


template<typename TSampler>
MaterialEvaluationOutput EvaluateMaterial(TSampler sampler, MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	MaterialEvaluationOutput output;

	TerrainInterface terrainInterface = SceneTerrain();

	float3 baseColor;
	float3 normal;
	float  metallic;
	float  roughness;
	float  occlusion;

	bool foundCachedMaterial = false;

#if !SPT_RAY_TRACING_SHADER 
	for (int i = 0; i < 8; ++i)
	{
		TerrrainMaterialCacheLOD matCacheLOD = terrainInterface.materialCache.lods[i];
		const float2 lodUV = (evalParams.worldLocation.xy - matCacheLOD.minBounds) * matCacheLOD.rcpRange;
		const float2 uv = evalParams.worldLocation.xy * matCacheLOD.rcpRange;

		if (all(lodUV > matCacheLOD.rcpResolution) && all(lodUV < 1.f - matCacheLOD.rcpResolution))
		{
			const float4 baseColorMetallic  = matCacheLOD.baseColorMetallic.SampleLevel(BindlessSamplers::LinearRepeat(), uv, 0);
			const float2 octNormals         = matCacheLOD.normals.SampleLevel(BindlessSamplers::LinearRepeat(), uv, 0);
			const float2 roughnessOcclusion = matCacheLOD.roughnessOcclusion.SampleLevel(BindlessSamplers::LinearRepeat(), uv, 0);

			baseColor = baseColorMetallic.xyz;
			metallic  = baseColorMetallic.w;
			normal    = OctahedronDecodeNormal(octNormals);
			roughness = roughnessOcclusion.x;
			occlusion = roughnessOcclusion.y;

			foundCachedMaterial = true;

			break;
		}
	}
#endif // !SPT_RAY_TRACING_SHADER

	if (!foundCachedMaterial)
	{
		normal = terrainInterface.GetNormal(evalParams.worldLocation.xy);
		terrainInterface.EvaluateFarLODMaterial(evalParams.worldLocation.xy, baseColor, roughness, metallic, occlusion);
	}

#if !SPT_RAY_TRACING_SHADER 
	if (all(evalParams.worldLocation.xy >= terrainInterface.grassFieldDef.worldSpaceMin)
		&& all(evalParams.worldLocation.xy <= terrainInterface.grassFieldDef.worldSpaceMax))
	{
		const TerrainMaterialsFactors materials = terrainInterface.GetMaterialsFactors(evalParams.worldLocation.xy);
		float grassDensity = 0.f;
		for (uint i = 0u; i < 4u; ++i)
		{
			const uint materialID = materials.materialIDs[i];
			if (materialID != IDX_NONE_8)
			{
				const TerrainMaterialEntry materialEntry = terrainInterface.materialData.matEntries.Load(materialID);
				grassDensity += (materialEntry.grassType != IDX_NONE_8 ? 1.f : 0.f) * materials.materialWeights[i];
			}
		}

		occlusion *= lerp(1.f, 0.4f, grassDensity);
	}
#endif // !SPT_RAY_TRACING_SHADER

	output.shadingNormal  = normal;
	output.geometryNormal = normal;
	output.roughness      = roughness;
	output.baseColor      = baseColor;
	output.metallic       = metallic;
	output.emissiveColor  = 0.f;
	output.occlusion      = occlusion;

	return output;
}


struct TerrainPOMData
{
	MaterialDepthData matDepth;
	float2 uv;
	float2 uvScale;
	float  pomStrenght;
};


TerrainPOMData GetTerrainPOMDepthData(MaterialEvaluationParameters evalParams)
{
	TerrainPOMData output;
	output.matDepth.depthTexture = SRVTexture2D<float>::Invalid();

	TerrainInterface terrainInterface = SceneTerrain();

	for (int i = 0; i < TERRAIN_POM_LODS_NUM; ++i)
	{
		TerrrainMaterialCacheLOD matCacheLOD = terrainInterface.materialCache.lods[i];
		const float2 lodUV = (evalParams.worldLocation.xy - matCacheLOD.minBounds) * matCacheLOD.rcpRange;
		if (all(saturate(lodUV * 2.f - 0.5f) == (lodUV * 2.f - 0.5f)))
		{
			const float2 uv = evalParams.worldLocation.xy * matCacheLOD.rcpRange;

			output.matDepth.maxDepthCm   = 0.06f;
			output.matDepth.depthTexture = matCacheLOD.pomDepth;
			output.uv      = uv;
			output.uvScale = matCacheLOD.rcpRange;

			float pomStrenght = 1.f;
			if (i == TERRAIN_POM_LODS_NUM - 1)
			{
				const float xStrength = max(lodUV.x, 1.f - lodUV.x) * 10.f - 0.1f;
				const float yStrength = max(lodUV.y, 1.f - lodUV.y) * 10.f - 0.1f;
				pomStrenght = saturate(min(xStrength, yStrength));
			}

			output.pomStrenght = pomStrenght;

			break;
		}
	}

	return output;
}

#endif // TERRAIN_MATERIAL_HLSLI
