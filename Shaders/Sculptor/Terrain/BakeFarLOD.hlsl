#include "SculptorShader.hlsli"

[[shader_params(BakeFarLODConstants, PARAMS_BAKE_FAR_L_O_D_CONSTANTS)]]

#define SPT_MATERIAL_DATA_TYPE    MaterialPBRData
#define SPT_MATERIAL_SHADER_PATH "Sculptor/Materials/DefaultPBR.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Terrain/TerrainMaterial.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void BakeFarLODCS(CS_INPUT input)
{
	//const float2 uv = (input.globalID.xy + 0.5f) / PARAMS_BAKE_FAR_L_O_D_CONSTANTS->resolution;

	//const float2 range = PARAMS_BAKE_FAR_L_O_D_CONSTANTS->maxBounds - PARAMS_BAKE_FAR_L_O_D_CONSTANTS->minBounds;

	//float3 baseColorSum = 0.f;
	//float3 propsSum = 0.f;
	//float samplesNum = 0.f;

	//const int sampleRadius = 3;

	//for (int x = -sampleRadius; x <= sampleRadius; ++x)
	//{
	//	for (int y = -sampleRadius; y <= sampleRadius; ++y)
	//	{
	//		const float2 offset = (float2(x, y) / sampleRadius) * 0.5f;

	//		const float2 sampleUV = uv + offset / PARAMS_BAKE_FAR_L_O_D_CONSTANTS->resolution;

	//		float3 worldLocation;
	//		worldLocation.xy = PARAMS_BAKE_FAR_L_O_D_CONSTANTS->minBounds + sampleUV * range;
	//		worldLocation.z  = 0.f;

	//		const float3 normal    = float3(0.f, 0.f, 1.f);
	//		const float3 tangent   = float3(1.f, 0.f, 0.f);
	//		const float3 bitangent = float3(0.f, 1.f, 0.f);

	//		MaterialEvaluationParameters evalParams;
	//		evalParams.normal        = normal;
	//		evalParams.tangent       = tangent;
	//		evalParams.bitangent     = bitangent;
	//		evalParams.hasTangent    = true;
	//		evalParams.uv            = worldLocation.xy;
	//		evalParams.worldLocation = worldLocation;
	//		evalParams.clipSpace     = 0.f;

	//		const TerrainMaterialsFactors materialFactors = SampleTerrainMaterialsMap(PARAMS_BAKE_FAR_L_O_D_CONSTANTS->materialsMap, worldLocation.xy);

	//		const TerrainMaterialEvaluationOutput materialEvalOutput = EvaluateTerrainMaterial(PARAMS_BAKE_FAR_L_O_D_CONSTANTS->materialsData, evalParams, PARAMS_BAKE_FAR_L_O_D_CONSTANTS->terrainMaterial, materialFactors);

	//		baseColorSum += materialEvalOutput.material.baseColor;
	//		propsSum += float3(materialEvalOutput.material.roughness, materialEvalOutput.material.metallic, materialEvalOutput.material.occlusion);
	//		samplesNum += 1.f;
	//	}
	//}

	//float3 baseColor = baseColorSum / samplesNum;
	//float3 props     = propsSum / samplesNum;

	//PARAMS_BAKE_FAR_L_O_D_CONSTANTS->rwFarLODBaseColor.Store(input.globalID.xy, baseColor);
	//PARAMS_BAKE_FAR_L_O_D_CONSTANTS->rwFarLODProps.Store(input.globalID.xy, props);
}
