#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]

[[shader_params(UpdateMaterialCacheConstants, u_constants)]]

#define SPT_MATERIAL_DATA_TYPE    MaterialPBRData
#define SPT_MATERIAL_SHADER_PATH "Sculptor/Materials/DefaultPBR.hlsli"

#define FULLSCREEN_FORCE_DEPTH 0.5f

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/FullScreen.hlsli"
#include "Utils/Packing.hlsli"
#include "Hashing.hlsli"
#include "Terrain/TerrainMaterial.hlsli"
#include "Terrain/SceneTerrain.hlsli"
#include "Terrain/TerrainHeightBasedBlending.hlsli"


struct PS_OUTPUT
{
	float4 baseColorMetallic  : SV_Target0;
	float2 normals            : SV_Target1;
	float2 roughnessOcclusion : SV_Target2;

#if RENDER_POM_DEPTH
	float pomDepth            : SV_Target3;
#if RENDER_DISPLACEMENT
	float displacement        : SV_Target4;
#endif // RENDER_DISPLACEMENT
#elif RENDER_DISPLACEMENT
	float displacement        : SV_Target3;
#endif // RENDER_POM_DEPTH
};


PS_OUTPUT RenderTerrainMaterialCacheFS(VS_OUTPUT input)
{
	TerrainInterface terrainInterface = SceneTerrain();

	const float2 lodMinUV = u_constants.minBounds / u_constants.range;
	const float2 lodUV = frac(input.uv - lodMinUV);

	float3 worldLocation;
	worldLocation.xy = u_constants.minBounds + lodUV * u_constants.range;
	worldLocation.z  = terrainInterface.GetHeight(worldLocation.xy);

	const float3 normal = terrainInterface.GetNormal(worldLocation.xy);
	const float3 tangent = terrainInterface.GetTangent(worldLocation.xy);
	const float3 bitangent = terrainInterface.GetBitangent(worldLocation.xy);
	const float2 uv = worldLocation.xy;

	MaterialEvaluationParameters evalParams;
	evalParams.normal        = normal;
	evalParams.tangent       = tangent;
	evalParams.bitangent     = bitangent;
	evalParams.hasTangent    = true;
	evalParams.uv            = uv;
	evalParams.worldLocation = worldLocation;
	evalParams.clipSpace     = 0.f;

	TerrainMaterialsFactors materialFactors = terrainInterface.GetMaterialsFactors(worldLocation.xy);
	ApplyHeightBasedWeighting(materialFactors, worldLocation.xy);

	const TerrainMaterialEvaluationOutput materialEvalOutput = EvaluateTerrainMaterial(evalParams, u_constants.terrainMaterial, materialFactors);

	const float2 encodedNormal = OctahedronEncodeNormal(materialEvalOutput.material.shadingNormal);

	PS_OUTPUT output;
	output.baseColorMetallic  = float4(materialEvalOutput.material.baseColor, materialEvalOutput.material.metallic);
	output.normals            = encodedNormal;
	output.roughnessOcclusion = float2(materialEvalOutput.material.roughness, materialEvalOutput.material.occlusion);
#if RENDER_POM_DEPTH
	output.pomDepth     = materialEvalOutput.pomDepth;
#endif // RENDER_POM_DEPTH

#if RENDER_DISPLACEMENT
	output.displacement = materialEvalOutput.displacement;
#endif // RENDER_DISPLACEMENT

	return output;
}
[[meta(debug_features)]]
