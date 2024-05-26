#ifndef SPECULAR_REFLECTIONS_COMMON_HLSLI
#define SPECULAR_REFLECTIONS_COMMON_HLSLI

#include "Shading/Shading.hlsli"

#define SPECULAR_TRACE_MAX_ROUGHNESS 0.05f
#define GLOSSY_TRACE_MAX_ROUGHNESS 0.9f


float3 SR_GGX_Specular(in float3 n, in float3 v, in float3 l, in float roughness, in float3 f0)
{
	float3 specular = 0.f;

	if(roughness >= SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		specular = GGX_Specular(n, v, l, roughness, f0);
	}
	else
	{
		const float3 h = normalize(v + l);
		const float dotVH = saturate(dot(v, h));
		const float3 f = F_Schlick(f0, dotVH);
		specular = f;
	}

	return specular;
}

#endif // SPECULAR_REFLECTIONS_COMMON_HLSLI
