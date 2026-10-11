#ifndef RAYTRACINGPAYLOAD_HLSLI
#define RAYTRACINGPAYLOAD_HLSLI

#include "Utils/Packing.hlsli"
#include "Materials/MaterialTypes.hlsli"
#include "Shading/BRDFs/BRDFsCommon.hlsli"

struct MaterialPayload
{
	uint  baseColorMetallic;
	float distance;
	half3 normal;
	half  roughness;
	half  clearCoatWeight;
	half  clearCoatRoughness;
	half3 emissive;

	[mutating] void Encode(in MaterialEvaluationOutput evaluatedMat)
	{
		normal             = half3(evaluatedMat.shadingNormal);
		roughness          = half(evaluatedMat.roughness);
		clearCoatWeight    = half(evaluatedMat.clearCoatWeight);
		clearCoatRoughness = half(evaluatedMat.clearCoatRoughness);
		baseColorMetallic  = PackFloat4x8(float4(evaluatedMat.baseColor.xyz, evaluatedMat.metallic));
		emissive           = half3(evaluatedMat.emissiveColor);
	}

	ShadedSurface AsShadedSurface()
	{
		ShadedSurface surface;
		surface.normal             = float3(normal);
		surface.roughness          = float(roughness);
		surface.clearCoatWeight    = float(clearCoatWeight);
		surface.clearCoatRoughness = float(clearCoatRoughness);

		const float4 baseColorMetallicUnpacked = UnpackFloat4x8(baseColorMetallic);

		ComputeSurfaceColor(baseColorMetallicUnpacked.xyz, baseColorMetallicUnpacked.w, OUT surface.albedo, OUT surface.f0);

		return surface;
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
