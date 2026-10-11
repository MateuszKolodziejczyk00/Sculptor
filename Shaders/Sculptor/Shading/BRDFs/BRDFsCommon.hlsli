#ifndef BRDFS_COMMON_HLSLI
#define BRDFS_COMMON_HLSLI

struct SurfaceGeometry
{
	float3 location;
	float3 geoNormal;
}


struct ShadedSurface
{
	float3 f0;
	float3 albedo;
	float3 normal;
	float  roughness;
	float clearCoatWeight;
	float clearCoatRoughness;
};


struct ShadingVectors
{
	float3 viewDir;
	float3 lightDir;
	float3 halfVector;

	float NoL;
	float NoV;
	float NoH;
	float VoH;

	__init(in float3 inViewDir, in float3 inLightDir, in float3 normal)
	{
		viewDir    = inViewDir;
		lightDir   = inLightDir;
		halfVector = normalize(inViewDir + inLightDir);

		NoL = saturate(dot(normal, lightDir));
		NoV = clamp(dot(normal, viewDir), 0.0001f, 1.f);
		NoH = clamp(dot(normal, halfVector), 0.0001f, 1.f);
		VoH = clamp(dot(viewDir, halfVector), 0.0001f, 1.f);
	}
};

struct ShadingSample
{
	float3 direction;
	float  pdf;
};


struct LobeProbabilities
{
	float diffuse;
	float specular;
	float clearCoat;
};


void ComputeSurfaceColor(in float3 baseColor, in float metallic, out float3 albedo, out float3 f0)
{
	albedo = baseColor * (1.0f - metallic);
	f0 = lerp(0.04f, baseColor, metallic);
}


float RoughnessToAlpha(in float roughness)
{
	return Pow2(roughness);
}


// Based on "Moving Frostbite to PBR" by Sebastien Lagarde", section 4.9.3
float3 GetSpecularDominantDirection(in float3 normal, in float3 reflected, float roughness)
{
	const float smoothness = 1.f - roughness;
	const float lerpFactor = smoothness * (sqrt(smoothness)  + roughness);
	return normalize(lerp(normal, reflected, lerpFactor));
}


// Based on GPU Gems 2, Chapter 32.3.2.1, "Accurate Real-Time Specular Reflections with Radiance Caching"
float3 EnvBRDFApprox(in float3 specularColor, in float alpha, in float NdotV)
{
	const float2x2 a = float2x2(0.99044f, -1.28514f,
	                            1.29678f, -0.755907f);

	const float3x3 b = float3x3(1.f,      2.92338f,  59.418f,
	                            20.3225f, -27.0302f, 222.592f,
	                            121.563f, 626.13f,   316.627f);

	const float2x2 c = float2x2(0.0365463f, 3.32707f,
	                            9.0632f,    -9.04756f);

	const float3x3 d = float3x3(1.f,      3.59685f,  -1.36772f,
	                            9.04401f, -16.3174f, 9.22949f,
	                            5.56589f, 19.7886f,  -20.2123f);

	const float num0   = mul(mul(float2(1.f, alpha), a), float2(1.f, NdotV));
	const float denom0 = mul(mul(float3(1.f, alpha, Pow3(alpha)), b), float3(1.f, NdotV, Pow3(NdotV)));

	const float num1   = mul(mul(float2(1.f, alpha), c), float2(1.f, NdotV));
	const float denom1 = mul(mul(float3(1.f, alpha, Pow3(alpha)), d), float3(1.f, NdotV, Pow3(NdotV)));

	return (num0 / denom0) + (num1 / denom1) * specularColor;
}


namespace brdf_utils
{

float IORToF0(in float ior)
{
	return ((ior - 1.f) / (ior + 1.f)) * ((ior - 1.f) / (ior + 1.f));
}


float F0ToIOR(in float f0)
{
	return (sqrt(f0) + 1.f) / (1.f - sqrt(f0));
}


float3 F0ToIOR(in float3 f0)
{
	return (sqrt(f0) + 1.f) / (1.f - sqrt(f0));
}

} // namespace brdf_utils


#endif // BRDFS_COMMON_HLSLI
