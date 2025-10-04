#ifndef RTGI_COMMON_HLSLI
#define RTGI_COMMON_HLSLI

#include "Shading/Shading.hlsli"

#define SPECULAR_TRACE_MAX_ROUGHNESS 0.01f


struct RTBRDF
{
	float3 specular;
	float3 diffuse;
};


RTBRDF RT_EvaluateBRDF(in float3 n, in float3 v, in float3 l, in float roughness, in float3 f0, in float3 albedo)
{
	RTBRDF brdf;

	if(roughness >= SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		float fresnel;
		brdf.specular = GGX_Specular(n, v, l, roughness, f0, OUT fresnel);
		brdf.diffuse = Diffuse_Lambert(albedo) * (1.f - fresnel);

	}
	else
	{
		const float3 h = normalize(v + l);
		const float dotVH = max(dot(v, h), 0.001f);
		const float3 f = F_Schlick(f0, dotVH);
		brdf.specular = f;
		brdf.diffuse = Diffuse_Lambert(albedo) * (1.f - f);
	}

	return brdf;
}

#endif // RTGI_COMMON_HLSLI
