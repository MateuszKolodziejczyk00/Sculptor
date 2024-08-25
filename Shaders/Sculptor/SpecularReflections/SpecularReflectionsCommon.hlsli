#ifndef SPECULAR_REFLECTIONS_COMMON_HLSLI
#define SPECULAR_REFLECTIONS_COMMON_HLSLI

#include "Shading/Shading.hlsli"

#define SPECULAR_TRACE_MAX_ROUGHNESS 0.01f


float3 SR_GGX_Specular(in float3 n, in float3 v, in float3 l, in float roughness, in float3 f0)
{
	float3 specular = 0.f;

	if(roughness >= SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		const float minRoughness = 0.01f;
		return GGX_Specular(n, v, l, max(roughness, minRoughness), f0);

	}
	else
	{
		const float3 h = normalize(v + l);
		const float dotVH = max(dot(v, h), 0.001f);
		const float3 f = F_Schlick(f0, dotVH);
		specular = f;
	}

	return specular;
}

#endif // SPECULAR_REFLECTIONS_COMMON_HLSLI
