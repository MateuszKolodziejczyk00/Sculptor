#include "SculptorShader.hlsli"

[[shader_params(BakeFarLODConstants, u_constants)]]

#define SPT_MATERIAL_DATA_TYPE    MaterialPBRData
#define SPT_MATERIAL_SHADER_PATH "Sculptor/Materials/DefaultPBR.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Materials/MaterialSystem.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void BakeFarLODCS(CS_INPUT input)
{
	const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData<SPT_MATERIAL_DATA_TYPE>(u_constants.materialsData, u_constants.terrainMaterial.terrainMaterial);

	const float2 uv = (input.globalID.xy + 0.5f) / u_constants.resolution;

	const float2 range = u_constants.maxBounds - u_constants.minBounds;

	float3 baseColorSum = 0.f;
	float3 propsSum = 0.f;
	float samplesNum = 0.f;

	const int sampleRadius = 3;

	for (int x = -sampleRadius; x <= sampleRadius; ++x)
	{
		for (int y = -sampleRadius; y <= sampleRadius; ++y)
		{
			const float2 offset = (float2(x, y) / sampleRadius) * 0.5f;

			const float2 sampleUV = uv + offset / u_constants.resolution;

			float3 worldLocation;
			worldLocation.xy = u_constants.minBounds + sampleUV * range;
			worldLocation.z  = 0.f;

			const float3 normal    = float3(0.f, 0.f, 1.f);
			const float3 tangent   = float3(1.f, 0.f, 0.f);
			const float3 bitangent = float3(0.f, 1.f, 0.f);

			const float2 uvScale = 1.f / 2.1f;

			MaterialEvaluationParameters evalParams;
			evalParams.normal        = normal;
			evalParams.tangent       = tangent;
			evalParams.bitangent     = bitangent;
			evalParams.hasTangent    = true;
			evalParams.uv            = worldLocation.xy * uvScale;
			evalParams.worldLocation = worldLocation;
			evalParams.clipSpace     = 0.f;

			const MaterialEvaluationOutput materialEvalOutput = EvaluateMaterial(evalParams, materialData);

			baseColorSum += materialEvalOutput.baseColor;
			propsSum += float3(materialEvalOutput.roughness, materialEvalOutput.metallic, materialEvalOutput.occlusion);
			samplesNum += 1.f;
		}
	}

	float3 baseColor = baseColorSum / samplesNum;
	float3 props     = propsSum / samplesNum;

	u_constants.rwFarLODBaseColor.Store(input.globalID.xy, baseColor);
	u_constants.rwFarLODProps.Store(input.globalID.xy, props);
}
