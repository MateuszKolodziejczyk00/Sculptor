#ifndef TERRAIN_MATERIAL_HLSLI
#define TERRAIN_MATERIAL_HLSLI

#include "SceneRendering/GPUScene.hlsli"
#include "Utils/Packing.hlsli"


CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    CustomOpacityOutput output;
    output.shouldDiscard = false;
    return output;
}


MaterialEvaluationOutput EvaluateMaterial(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	const float3 baseColor = lerp(float3(0.1f, 0.3f, 0.1f), float3(0.4f, 0.6f, 0.4f), saturate(evalParams.worldLocation.z * 0.03f));
	const float  metallic  = 0.f;
	const float3 normal    = evalParams.normal;
	const float  roughness = 0.88f;

	MaterialEvaluationOutput output;

	output.shadingNormal  = normal;
	output.geometryNormal = normal;
	output.roughness      = roughness;
	output.baseColor      = baseColor;
	output.metallic       = metallic;
	output.emissiveColor  = 0.f;
	output.occlusion      = 0.f;

	return output;
}

#endif // TERRAIN_MATERIAL_HLSLI
