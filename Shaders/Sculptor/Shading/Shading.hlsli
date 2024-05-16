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


void ComputeSurfaceColor(in float3 baseColor, in float metallic, out float3 diffuseColor, out float3 specularColor)
{
	diffuseColor = baseColor * (1.0f - metallic);
	specularColor = lerp(0.04f, baseColor, metallic);
}

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

float GGXMasking(in float a2, in float dotNL)
{
	return 2.0f / (1.0f + sqrt((1.0f - a2) + a2 / Pow2(dotNL)));
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

struct LightingContribution
{
	static LightingContribution Create(in float3 inSceneLuminance, in float3 inEyeAdaptationLuminance)
	{
		LightingContribution res;
		res.sceneLuminance         = inSceneLuminance;
		res.eyeAdaptationLuminance = inEyeAdaptationLuminance;
		return res;
	}

	static LightingContribution Create(in float3 inLuminance)
	{
		return Create(inLuminance, inLuminance);
	}

	LightingContribution operator*(float3 value)
	{
		LightingContribution res;
		res.sceneLuminance         = sceneLuminance * value;
		res.eyeAdaptationLuminance = eyeAdaptationLuminance * value;
		return res;
	}

	float3 sceneLuminance;
	float3 eyeAdaptationLuminance;
};


LightingContribution DoShading(in ShadedSurface surface, in float3 lightDir, in float3 viewDir, in float3 peakIlluminance)
{
	const float a = max(Pow2(surface.roughness), 0.01f);

	const float dotNL = saturate(dot(surface.shadingNormal, lightDir));
	const float3 h = normalize(viewDir + lightDir);
	const float dotVH = dot(viewDir, h);

	const float3 fresnel = F_Schlick(surface.specularColor, dotVH);

	const float3 diffuse = Diffuse_Lambert(surface.diffuseColor);
	const float3 specular = GGX_Specular(a, surface.shadingNormal, h, viewDir, lightDir);

	const float3 sceneLuminance = (diffuse + specular * fresnel) * dotNL * peakIlluminance;
	const float3 eyeAdaptationLuminance = Diffuse_Lambert(1.f) * dotNL * peakIlluminance;

	return LightingContribution::Create(sceneLuminance, eyeAdaptationLuminance);
}

float GeometrySchlickGGX(in float NdotV, in float a)
{
	const float k = (a * a) / 2.0;

	const float nom   = NdotV;
	const float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	const float NdotV = max(dot(N, V), 0.0);
	const float NdotL = max(dot(N, L), 0.0);
	const float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	const float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
} 


float3 ImportanceSampleGGX(in float2 Xi, in float3 N, in float roughness)
{
	float a = roughness*roughness;
	
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
	// from tangent-space floattor to world-space sample floattor
	float3 up        = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent   = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	
	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}



// Based on: https://auzaiffe.wordpress.com/2024/04/15/vndf-importance-sampling-an-isotropic-distribution/
float3 SampleVNDFIsotropic(float2 u, float3 wi, float alpha, float3 n)
{
	// decompose the floattor in parallel and perpendicular components
	float3 wi_z = -n * dot(wi, n);
	float3 wi_xy = wi + wi_z;
 
	// warp to the hemisphere configuration
	float3 wiStd = -normalize(alpha * wi_xy + wi_z);
 
	// sample a spherical cap in (-wiStd.z, 1]
	float wiStd_z = dot(wiStd, n);
	float z = 1.0 - u.y * (1.0 + wiStd_z);
	float sinTheta = sqrt(saturate(1.0f - z * z));
	float phi = TWO_PI * u.x - PI;
	float x = sinTheta * cos(phi);
	float y = sinTheta * sin(phi);
	float3 cStd = float3(x, y, z);
 
	// reflect sample to align with normal
	float3 up = float3(0, 0, 1.000001); // Used for the singularity
	float3 wr = n + up;
	float3 c = dot(wr, cStd) * wr / wr.z - cStd;
 
	// compute halfway direction as standard normal
	float3 wmStd = c + wiStd;
	float3 wmStd_z = n * dot(n, wmStd);
	float3 wmStd_xy = wmStd_z - wmStd;
	 
	// return final normal
	return normalize(alpha * wmStd_xy + wmStd_z);
}


float PDFVNDFIsotrpic(float3 wo, float3 wi, float alpha, float3 n)
{
	float alphaSquare = alpha * alpha;
	float3 wm = normalize(wo + wi);
	float zm = dot(wm, n);
	float zi = dot(wi, n);
	float nrm = rsqrt((zi * zi) * (1.0f - alphaSquare) + alphaSquare);
	float sigmaStd = (zi * nrm) * 0.5f + 0.5f;
	float sigmaI = sigmaStd / nrm;
	float nrmN = (zm * zm) * (alphaSquare - 1.0f) + 1.0f;
	return alphaSquare / (PI * 4.0f * nrmN * nrmN * sigmaI);
}


float3 IntegrateSpecularGGX(in float3 specularColor, in float3 shadingNormal, in float roughness, in float3 lightDir, in float3 viewDir, in float3 peakIlluminance)
{
	const float a = max(roughness, 0.01f);
	const float a2 = Pow2(a);

	const float dotNL = saturate(dot(shadingNormal, lightDir));
	const float3 h = normalize(viewDir + lightDir);
	const float dotVH = clamp(dot(viewDir, h), 0.001f, 1.f);
	const float dotNV = clamp(dot(shadingNormal, viewDir), 0.001f, 1.f);

	const float3 fresnel = F_Schlick(specularColor, dotVH);

	return (specularColor * fresnel * GGXMasking(a2, dotNL)) * peakIlluminance;
}


// Based on "Moving Frostbite to PBR" by Sebastien Lagarde", section 4.9.3
float3 GetSpecularDominantDirection(in float3 normal, in float3 reflected, float roughness)
{
	const float smoothness = 1.f - roughness;
	const float lerpFactor = smoothness * (sqrt(smoothness)  + roughness);
	return normalize(lerp(normal, reflected, lerpFactor));
}
