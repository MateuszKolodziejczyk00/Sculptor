#ifndef TERRAIN_MATERIAL_HLSLI
#define TERRAIN_MATERIAL_HLSLI

#include "Materials/MaterialSystem.hlsli"
#include "Terrain/TerrainTypes.hlsli"


struct TerrainMaterialEvaluationOutput
{
	MaterialEvaluationOutput material;
	float                    pomDepth;
	float                    displacement;
};


TerrainMaterialEvaluationOutput EvaluateTerrainMaterial(in MaterialUnifiedData materialsData, in MaterialEvaluationParameters evalParams, in TerrainMaterialData terrainMaterials, in TerrainMaterialsFactors materialsFactors)
{
	TerrainMaterialEvaluationOutput output = (TerrainMaterialEvaluationOutput)0.f;
	float totalWeight = 0.f;

	const float2 uv = evalParams.uv;

	for (int i = 0; i < 4; ++i)
	{
		const uint materialID = materialsFactors.materialIDs[i];
		const float weight = materialsFactors.materialWeights[i];

		if (weight > 0.f)
		{
			const TerrainMaterialEntry materialEntry = terrainMaterials.matEntries.Load(materialID);

			const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData<SPT_MATERIAL_DATA_TYPE>(materialsData, materialEntry.dataHandle);

			evalParams.uv = uv * materialEntry.uvScale;

			TerrainDetilingSampler sampler = TerrainDetilingSampler::Initialize(evalParams.uv);
			const MaterialEvaluationOutput materialEvalOutput = EvaluateMaterial(sampler, evalParams, materialData);

			output.material.baseColor      += materialEvalOutput.baseColor * weight;
			output.material.roughness      += materialEvalOutput.roughness * weight;
			output.material.metallic       += materialEvalOutput.metallic * weight;
			output.material.occlusion      += materialEvalOutput.occlusion * weight;
			output.material.shadingNormal  += materialEvalOutput.shadingNormal * weight;
			output.material.geometryNormal += materialEvalOutput.geometryNormal * weight;

#if defined(MATERIAL_DEPTH_DATA_ACCESSOR)
			MaterialDepthData materialDepthData = materialData.MATERIAL_DEPTH_DATA_ACCESSOR;
			output.pomDepth += sampler.Sample(materialDepthData.depthTexture, BindlessSamplers::LinearRepeat(), evalParams.uv) * weight;
#endif // MATERIAL_DEPTH_DATA_ACCESSOR

#if defined(MATERIAL_DISPLACEMENT_DATA_ACCESSOR)
			MaterialDisplacementData materialDisplacementData = materialData.MATERIAL_DISPLACEMENT_DATA_ACCESSOR;
			if (materialDisplacementData.displacementTexture.IsValid())
			{
				output.displacement += sampler.Sample(materialDisplacementData.displacementTexture, BindlessSamplers::LinearRepeat(), evalParams.uv) * weight;
			}
#endif // MATERIAL_DISPLACEMENT_DATA_ACCESSOR

			totalWeight += weight;
		}
	}

	const float rcpTotalWeight = 1.f / totalWeight;

	if (totalWeight > 0.f)
	{
		output.material.baseColor      *= rcpTotalWeight;
		output.material.roughness      *= rcpTotalWeight;
		output.material.metallic       *= rcpTotalWeight;
		output.material.occlusion      *= rcpTotalWeight;
		output.material.shadingNormal  = normalize(output.material.shadingNormal * rcpTotalWeight);
		output.material.geometryNormal = normalize(output.material.geometryNormal * rcpTotalWeight);

		output.pomDepth *= rcpTotalWeight;
		output.displacement *= rcpTotalWeight;
	}

	return output;
}


#ifdef DS_RenderSceneDS
TerrainMaterialEvaluationOutput EvaluateTerrainMaterial(in MaterialEvaluationParameters evalParams, in TerrainMaterialData terrainMaterials, in TerrainMaterialsFactors materialsFactors)
{
	return EvaluateTerrainMaterial(GPUMaterials().data, evalParams, terrainMaterials, materialsFactors);
}
#endif // DS_RenderSceneDS

#endif // TERRAIN_MATERIAL_HLSLI
