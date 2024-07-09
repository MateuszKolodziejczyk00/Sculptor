#ifndef	RTGBUFFER_HLSLI
#define RTGBUFFER_HLSLI

#include "Utils/Packing.hlsli"

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
	
	uint hitType;

	float hitDistance;
};


struct RTGBufferData
{
	float2 normal;
	float4 baseColorMetallic;
	float roughness;
	float hitDistance;
};


RTGBufferData PackRTGBuffer(in RayHitResult hitResult)
{
	RTGBufferData data;

	if(hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
	{
		data.hitDistance = hitResult.hitDistance;
		data.normal            = OctahedronEncodeNormal(hitResult.normal);
		data.baseColorMetallic = float4(hitResult.baseColor, hitResult.metallic);
		data.roughness         = hitResult.roughness;
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


RayHitResult UnpackRTGBuffer(in RTGBufferData data)
{
	RayHitResult hitResult;

	if(isnan(data.hitDistance))
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_NO_HIT;
	}
	else if(data.hitDistance >= 0.f)
	{
		hitResult.normal    = OctahedronDecodeNormal(data.normal);
		hitResult.baseColor = data.baseColorMetallic.xyz;
		hitResult.metallic  = data.baseColorMetallic.w;
		hitResult.roughness = data.roughness;
		hitResult.hitType   = RTGBUFFER_HIT_TYPE_VALID_HIT;
	}
	else
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_BACKFACE;
	}

	hitResult.hitDistance = data.hitDistance;
	return hitResult;
}


template<template<typename TComponent> class TTextureType>
struct RTGBuffer
{
	TTextureType<float2> hitNormal;
	TTextureType<float4> hitBaseColorMetallic;
	TTextureType<float>  hitRoughness;
	TTextureType<float>  rayDistance;
};


void WriteRTGBuffer(in uint2 pixel, in RTGBufferData data, in RTGBuffer<RWTexture2D> rtgBuffer)
{
	if(!isnan(data.hitDistance) && data.hitDistance >= 0.f)
	{
		rtgBuffer.hitNormal[pixel]            = data.normal;
		rtgBuffer.hitBaseColorMetallic[pixel] = data.baseColorMetallic;
		rtgBuffer.hitRoughness[pixel]         = data.roughness;
	}

	rtgBuffer.rayDistance[pixel] = data.hitDistance;
}


RTGBufferData ReadRTGBuffer(in uint2 pixel, in RTGBuffer<Texture2D> rtgBuffer)
{
	RTGBufferData data;
	data.hitDistance = rtgBuffer.rayDistance[pixel];

	if(!isnan(data.hitDistance) && data.hitDistance >= 0.f)
	{
		data.normal            = rtgBuffer.hitNormal[pixel];
		data.baseColorMetallic = rtgBuffer.hitBaseColorMetallic[pixel];
		data.roughness         = rtgBuffer.hitRoughness[pixel];
	}

	return data;
}

#endif // RTGBUFFER_HLSLI
