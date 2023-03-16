struct ShadedSurface
{
    float3  location;
    float3  shadingNormal;
    float3  geometryNormal;
    float3  specularColor;
    float3  diffuseColor;
    float2  uv;
    float   roughness;
    float   linearDepth;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Diffuse =======================================================================================

float3 Diffuse_Lambert(in float3 diffuseColor)
{
	return diffuseColor * INV_PI;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Frenel ========================================================================================

float3 F_Schlick(in float3 f0, in float dotVH)
{
	float Fc = Pow5(1.0f - dotVH);
	return Fc + (1.0f - Fc) * f0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GGX Visibility ================================================================================

float GGX_V1(in float a2, in float nDotX)
{
    return 1.0f / (nDotX + sqrt(a2 + (1 - a2) * nDotX * nDotX));
}

float GGXVisibility(in float a2, in float dotNL, in float dotNV)
{
    return GGX_V1(a2, dotNL) * GGX_V1(a2, dotNV);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GGX Specular ==================================================================================

float GGX_Specular(in float roughness, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float dotNH = clamp(dot(n, h), 0.001f, 1.f);
    float dotNL = clamp(dot(n, l), 0.001f, 1.f);
    float dotNV = clamp(dot(n, v), 0.001f, 1.f);

    float a2 = Pow2(roughness);

    float d = a2 / (PI * Pow2(dotNH * dotNH * (a2 - 1.f) + 1.f));

    return d * GGXVisibility(a2, dotNL, dotNV);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Shading function ==============================================================================

float3 DoShading(in ShadedSurface surface, in float3 lightDir, in float3 viewDir, in float3 peakIrradiance)
{
    const float dotNL = saturate(dot(surface.shadingNormal, lightDir));
    const float3 h = normalize(surface.shadingNormal + viewDir);
    const float dotVH = dot(viewDir, h);

    const float3 fresnel = F_Schlick(surface.specularColor, dotVH);
    
    const float3 diffuse = Diffuse_Lambert(surface.diffuseColor);
    const float3 specular = GGX_Specular(surface.roughness, surface.shadingNormal, h, viewDir, lightDir);

    return (diffuse + specular * fresnel) * dotNL * peakIrradiance;
}
