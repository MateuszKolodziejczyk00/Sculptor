#ifndef	RTGBUFFER_HLSLI
#define RTGBUFFER_HLSLI

#include "Utils/Packing.hlsli"

[[shader_struct(RTHitMaterialInfo)]]

#define RTGBUFFER_HIT_TYPE_INVALID   (0)
#define RTGBUFFER_HIT_TYPE_VALID_HIT (1)
#define RTGBUFFER_HIT_TYPE_BACKFACE  (2)
#define RTGBUFFER_HIT_TYPE_NO_HIT    (3)

struct RayHitResult
{
	static RayHitResult CreateEmpty()
	{
		RayHitResult result;
		result.normal      = 0.f;
		result.roughness   = 0.f;
		result.baseColor   = 0.f;
		result.metallic    = 0.f;
		result.hitType     = RTGBUFFER_HIT_TYPE_INVALID;
		result.hitDistance = 0.f;
		return result;
	}

	float3 normal;
	float roughness;
	float3 baseColor;
	float metallic;

	float3 emissive;
	
	uint hitType;

	float hitDistance;
};


uint PackHitNormal(in float3 normal)
{
	const float2 encodedNormal = OctahedronEncodeNormal(normal);
	return PackHalf2x16Norm(encodedNormal);
}


float3 UnpackHitNormal(in uint packedNormal)
{
	const float2 encodedNormal = UnpackHalf2x16Norm(packedNormal);
	return OctahedronDecodeNormal(encodedNormal);
}


uint PackEmissive(in float3 emissive)
{
	return EncodeRGBToLogLuv(emissive);
}


float3 UnpackEmissive(in uint packedEmissive)
{
	return DecodeRGBFromLogLuv(packedEmissive);
}


RTHitMaterialInfo PackRTGBuffer(in RayHitResult hitResult)
{
	RTHitMaterialInfo data;

	if(hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
	{
		data.hitDistance       = hitResult.hitDistance;
		data.normal            = PackHitNormal(hitResult.normal);
		data.baseColorMetallic = PackFloat4x8(float4(hitResult.baseColor, hitResult.metallic));
		data.roughness         = PackFloatNorm(hitResult.roughness);
		data.emissive          = PackEmissive(hitResult.emissive);
	}
	else if (hitResult.hitType == RTGBUFFER_HIT_TYPE_BACKFACE)
	{
		data.hitDistance = -1.f;
	}
	else if (hitResult.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
	{
		data.hitDistance = SPT_NAN;
	}

	return data;
}


RayHitResult UnpackRTGBuffer(in RTHitMaterialInfo data)
{
	RayHitResult hitResult;

	if(isnan(data.hitDistance))
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_NO_HIT;
	}
	else if(data.hitDistance >= 0.f)
	{
		const float4 baseColorMetallic = UnpackFloat4x8(data.baseColorMetallic);

		hitResult.normal    = UnpackHitNormal(data.normal);
		hitResult.baseColor = baseColorMetallic.rgb;
		hitResult.metallic  = baseColorMetallic.w;
		hitResult.roughness = UnpackFloatNorm(data.roughness);
		hitResult.hitType   = RTGBUFFER_HIT_TYPE_VALID_HIT;
		hitResult.emissive  = UnpackEmissive(data.emissive);
	}
	else
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_BACKFACE;
	}

	hitResult.hitDistance = data.hitDistance;
	return hitResult;
}

#endif // RTGBUFFER_HLSLI
