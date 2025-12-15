#ifndef RAYTRACINGPAYLOAD_HLSLI
#define RAYTRACINGPAYLOAD_HLSLI

#include "Utils/Packing.hlsli"
#include "Materials/MaterialTypes.hlsli"

struct MaterialPayload
{
	uint  baseColorMetallic;
	float distance;
	half3 normal;
	half  roughness;
	half3 emissive;

	void Encode(in MaterialEvaluationOutput evaluatedMat)
	{
		normal            = half3(evaluatedMat.shadingNormal);
		roughness         = half(evaluatedMat.roughness);
		baseColorMetallic = PackFloat4x8(float4(evaluatedMat.baseColor.xyz, evaluatedMat.metallic));
		emissive          = half3(evaluatedMat.emissiveColor);
	}
};


struct VisibilityPayload
{
	bool isMiss;
	bool isValidHit; // false in case of backface hit for non-double-sided materials
};


struct RayPayloadData
{
#ifdef RT_MATERIAL_TRACING
	MaterialPayload material;
#endif // RT_MATERIAL_TRACING

	VisibilityPayload visibility;
	
	static RayPayloadData Init()
	{
		RayPayloadData payload;
		payload.visibility.isMiss     = false;
		payload.visibility.isValidHit = false;
		return payload;
	}
};

#endif // RAYTRACINGPAYLOAD_HLSLI
