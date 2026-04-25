#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]

[[shader_params(UpdateMaterialCacheConstants, u_constants)]]

#define SPT_MATERIAL_DATA_TYPE    MaterialPBRData
#define SPT_MATERIAL_SHADER_PATH "Sculptor/Materials/DefaultPBR.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/FullScreen.hlsli"
#include "Utils/Packing.hlsli"
#include "Materials/MaterialSystem.hlsli"
#include "Terrain/SceneTerrain.hlsli"


struct PS_OUTPUT
{
	float4 baseColorMetallic  : SV_Target0;
	float2 normals            : SV_Target1;
	float2 occlusionRoughness : SV_Target2;
};


PS_OUTPUT RenderTerrainMaterialCacheFS(VS_OUTPUT input)
{
	TerrainInterface terrainInterface = SceneTerrain();

	const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(u_constants.terrainMaterial.terrainMaterial);

	float3 worldLocation;
	worldLocation.xy = u_constants.minBounds + input.uv * u_constants.range;
	worldLocation.z  = terrainInterface.GetHeight(worldLocation.xy);

	const float3 normal = terrainInterface.GetNormal(worldLocation.xy);
	const float3 tangent = terrainInterface.GetTangent(worldLocation.xy);
	const float3 bitangent = terrainInterface.GetBitangent(worldLocation.xy);

	const float2 uvScale = 1.f / 2.1f;

	const float2 uv = worldLocation.xy * uvScale;

	MaterialEvaluationParameters evalParams;
	evalParams.normal        = normal;
	evalParams.tangent       = tangent;
	evalParams.bitangent     = bitangent;
	evalParams.hasTangent    = true;
	evalParams.uv            = uv;
	evalParams.worldLocation = worldLocation;
	evalParams.clipSpace     = 0.f;

	const MaterialEvaluationOutput materialEvalOutput = EvaluateMaterial(evalParams, materialData);

	const float2 encodedNormal = OctahedronEncodeNormal(materialEvalOutput.shadingNormal);

	PS_OUTPUT output;
	output.baseColorMetallic  = float4(materialEvalOutput.baseColor, materialEvalOutput.metallic);
	output.normals            = encodedNormal;
	output.occlusionRoughness = float2(materialEvalOutput.occlusion, materialEvalOutput.roughness);

	return output;
}
