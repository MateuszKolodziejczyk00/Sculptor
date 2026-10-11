#ifndef SPECULAR_HLSLI
#define SPECULAR_HLSLI

#include "Shading/BRDFs/BRDFsCommon.hlsli"

namespace brdf_utils
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// Fresnel =======================================================================================

float3 F_Schlick(in float3 f0, in float dotVH)
{
	float Fc = Pow5(1.0f - dotVH);
	return Fc + (1.0f - Fc) * f0;
}

float3 F_avg_Schlick(in float3 f0)
{
	// 2 * Integral F_Schlick(f0, x) * x dx from 0 to 1
	// https://fpsunflower.github.io/ckulla/data/s2017_pbs_imageworks_slides_v2.pdf
	// slide 15
	return (20.f * f0 + 1.f) / 21.f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GGX Visibility ================================================================================

// Contains (2 / NoX) factor (in uncorrelated form, it's 4 /(NoL * NoV) factor) from the microfacet BRDF equation
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

// Based on: Moving Frostbite to PBR by Sebastien Lagarde
//  Contains 1/ (4 * dotNL * dotNV) factor from the microfacet BRDF equation
float V_SmithGGXCorrelated(in float a2, in float dotNL, in float dotNV)
{
	const float lambdaGGXV = dotNL * sqrt((-dotNV * a2 + dotNV) * dotNV + a2);
	const float lambdaGGXL = dotNV * sqrt((-dotNL * a2 + dotNL) * dotNL + a2);
	
	return 0.5f / ( lambdaGGXV + lambdaGGXL );
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GGX Distribution ==============================================================================

float D_GGX(in float a2, in float dotNH)
{
	const float f = (dotNH * a2 - dotNH) * dotNH + 1.0f;
	return a2 / (PI * Pow2(f));
}

// Based on: https://auzaiffe.wordpress.com/2024/04/15/vndf-importance-sampling-an-isotropic-distribution/
namespace vndf_isotropic
{
float3 Sample(float2 u, float3 wi, float alpha, float3 n)
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
float PDF(float3 wo, float3 wi, float alpha, float3 n)
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
} // vndf_isotropic

static float3 clearCoatF0 = float3(0.04f, 0.04f, 0.04f);

} // brdf_utils


interface ISpecularBRDF
{
	static float3 F(in ShadedSurface surface, in float VoX);

	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors, out float3 fresnel);

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng);

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir);
};


interface IClearCoatBRDF
{
	static float3 F(in ShadedSurface surface, in float VoX);

	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors);

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng);

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir);
};


struct GGXSpecularBRDF<let enableMultiscattering : bool, let enableEnergyConservingThroughput : bool> : ISpecularBRDF
{
	static float3 F(in ShadedSurface surface, in float VoX)
	{
		return brdf_utils::F_Schlick(surface.f0, VoX);
	}

	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors, out float3 throughput)
	{
		if(shadingVectors.NoL <= 0.f || shadingVectors.NoV <= 0.f)
		{
			throughput = 0.f; // keep diffuse?
			return 0.0f;
		}

		const float a  = RoughnessToAlpha(surface.roughness);
		const float a2 = max(Pow2(a), 1e-7f);

		const float3 f   = F(surface, shadingVectors.VoH);
		const float  d   = brdf_utils::D_GGX(a2, shadingVectors.NoH);
		const float  vis = brdf_utils::V_SmithGGXCorrelated(a2, shadingVectors.NoL, shadingVectors.NoV);

		float3 multiscatteringTerm;

		if (enableMultiscattering)
		{
			const float E_avg      = GetAverageEnergy(surface);
			const float E_incident = GetPreservedEnergy(surface, shadingVectors.NoL);
			const float E_outgoing = GetPreservedEnergy(surface, shadingVectors.NoV);

			const float f_ms = (E_incident * E_outgoing) / max(PI * (1.f - E_avg), 1e-4f);

			const float3 F_avg = brdf_utils::F_avg_Schlick(surface.f0);
			const float3 missingEnergyFactor = (Pow2(F_avg) * E_avg) / max(1.f - F_avg * (1.f - E_avg), 1e-4f);

			multiscatteringTerm = f_ms * missingEnergyFactor;

			if (enableEnergyConservingThroughput)
			{
				throughput = GetEnergyConservingThroughput(surface, shadingVectors);
			}
			else
			{
				throughput = 1.f - f;
			}
		}
		else
		{
			multiscatteringTerm = 0.f;
			throughput = 1.f - f;
		}

		return f * d * vis + multiscatteringTerm;
	}

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng)
	{
		const float a = RoughnessToAlpha(roughness);

		const float3 h = brdf_utils::vndf_isotropic::Sample(rng, viewDir, a, normal);
		const float3 reflectedRay = reflect(-viewDir, h);

		ShadingSample sample;
		sample.direction = reflectedRay;
		sample.pdf       = brdf_utils::vndf_isotropic::PDF(sample.direction, viewDir, a, normal);

		return sample;
	}

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir)
	{
		return brdf_utils::vndf_isotropic::PDF(sampledDir, viewDir, RoughnessToAlpha(roughness), normal);
	}

	static float GetAverageEnergy(in ShadedSurface surface)
	{
		return GLOBALS->MS_GGX_E_avg_LUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(surface.roughness, 0.f), 0.f);
	}

	static float GetPreservedEnergy(in ShadedSurface surface, in float NoX)
	{
		return GLOBALS->MS_GGX_E_LUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NoX, surface.roughness), 0.f);
	}

	static float GetEnergyConservingThroughput(in float f0, in float roughness, in ShadingVectors shadingVectors)
	{
		const float3 uvw0 = float3(shadingVectors.NoV, roughness, f0);
		const float3 uvw1 = float3(shadingVectors.NoL, roughness, f0);

		const float t0 = GLOBALS->MS_GGX_THROUGHPUT_LUT.SampleLevel(BindlessSamplers::LinearClampEdge(), uvw0, 0.f);
		const float t1 = GLOBALS->MS_GGX_THROUGHPUT_LUT.SampleLevel(BindlessSamplers::LinearClampEdge(), uvw1, 0.f);

		return t0 * t1;
	}

	static float3 GetEnergyConservingThroughput(in ShadedSurface surface, in ShadingVectors shadingVectors)
	{
		float3 throughput;
		throughput.x = GetEnergyConservingThroughput(surface.f0.x, surface.roughness, shadingVectors);
		throughput.y = GetEnergyConservingThroughput(surface.f0.y, surface.roughness, shadingVectors);
		throughput.z = GetEnergyConservingThroughput(surface.f0.z, surface.roughness, shadingVectors);
		return throughput;
	}
};


struct GGXClearCoatBRDF : IClearCoatBRDF
{
	static float3 F(in ShadedSurface surface, in float VoX)
	{
		return brdf_utils::F_Schlick(brdf_utils::clearCoatF0, VoX);
	}

	// Eq. 99 from https://academysoftwarefoundation.github.io/OpenPBR
	// The fit is accurate to ~0.2% only for relativeIOR in [1, 3], values below 1 are derived from their reciprocal
	static float AverageFresnelApprox(in float relativeIOR)
	{
		const bool isBelowOne = relativeIOR < 1.f;
		const float eta = clamp(isBelowOne ? rcp(relativeIOR) : relativeIOR, 1.f, 3.f);

		const float averageFresnel = log((10893.f * eta - 1438.2f) / (-774.4f * Pow2(eta) + 10212.f * eta + 1.f));

		return isBelowOne ? 1.f - Pow2(relativeIOR) * (1.f - averageFresnel) : averageFresnel;
	}

	// Returns the factor that the base BSDF has to be multiplied by to account for the multiple internal
	// reflections inside the clear coat. Eq. 64 from https://academysoftwarefoundation.github.io/OpenPBR
	static float3 ComputeDarkeningFactor(in ShadedSurface surface, in ShadingVectors shadingVectors)
	{
		const float coatIOR = 1.5f;

		//// Internal diffuse reflection coefficient. Eq. 65 gives the rough base limit, Eq. 66 the smooth base one,
		//// where the specular reflection off the base rarely generates TIR at the coat boundary
		//const float kRough  = saturate(1.f - (1.f - AverageFresnelApprox(coatRelativeIOR)) / Pow2(coatRelativeIOR));

		//// Eq. 68 from https://academysoftwarefoundation.github.io/OpenPBR
		const float baseFresnel   = saturate(Luminance(brdf_utils::F_Schlick(surface.f0, shadingVectors.NoV)));
		const float baseRoughness = lerp(1.f, surface.roughness, baseFresnel);


		const float averageFresnel = AverageFresnelApprox(coatIOR);

		// Eq. 65 from https://academysoftwarefoundation.github.io/OpenPBR
		const float k_r = 1.f - (1.f - averageFresnel)  / Pow2(coatIOR);
		//const float3 k_s = brdf_utils::F_Schlick(brdf_utils::clearCoatF0, shadingVectors.NoV).x;

		//const float3 k = lerp(k_s, k_r, baseRoughness);
		const float k = k_r;

		const float3 incidenceAlbedo = saturate(surface.albedo + surface.f0);

		return (1.f - k) / max(1.f - incidenceAlbedo * k, 1e-4f);
	}

	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors)
	{
		if(shadingVectors.NoL <= 0.f || shadingVectors.NoV <= 0.f)
		{
			return 0.0f;
		}

		const float a  = RoughnessToAlpha(surface.clearCoatRoughness);
		const float a2 = max(Pow2(a), 1e-7f);

		const float3 f   = F(surface, shadingVectors.VoH);
		const float  d   = brdf_utils::D_GGX(a2, shadingVectors.NoH);
		const float  vis = brdf_utils::V_SmithGGXCorrelated(a2, shadingVectors.NoL, shadingVectors.NoV);

		return f * d * vis;
	}

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng)
	{
		const float a = RoughnessToAlpha(roughness);

		const float3 h = brdf_utils::vndf_isotropic::Sample(rng, viewDir, a, normal);
		const float3 reflectedRay = reflect(-viewDir, h);

		ShadingSample sample;
		sample.direction = reflectedRay;
		sample.pdf       = brdf_utils::vndf_isotropic::PDF(viewDir, sample.direction, a, normal);

		return sample;
	}

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir)
	{
		return brdf_utils::vndf_isotropic::PDF(viewDir, sampledDir, RoughnessToAlpha(roughness), normal);
	}
};

#endif // SPECULAR_HLSLI
