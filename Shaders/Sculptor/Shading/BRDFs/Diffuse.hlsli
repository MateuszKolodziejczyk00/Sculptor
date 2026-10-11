#ifndef DIFFUSE_HLSLI
#define DIFFUSE_HLSLI

#include "Shading/BRDFs/BRDFsCommon.hlsli"
#include "Utils/Random.hlsli"

namespace brdf_utils
{

namespace lambert
{

float3 Evaluate(in float3 diffuseColor)
{
	return diffuseColor * INV_PI;
}

} // lambert

namespace eon
{

#include "EON/EON.hlsli"

} // eon

} // brdf_utils

interface IDiffuseBRDF
{
	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors);

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng);

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir);
};


struct LambertDiffuseBRDF : IDiffuseBRDF
{
	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors)
	{
		return brdf_utils::lambert::Evaluate(surface.albedo);
	}

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng)
	{
		const float3x3 tangentToWorld = TangentFrameToWorld(normal);

		ShadingSample sample;
		sample.direction = RandomVectorInCosineWeightedHemisphere(tangentToWorld, rng, OUT sample.pdf);

		return sample;
	}

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir)
	{
		return PDFHemisphereCosineWeighted(normal, sampledDir);
	}
};


// This leads to some fireflies on grazing angles
#define EON_USE_EON_SAMPLING 0


struct EONDiffuseBRDF : IDiffuseBRDF
{
	static float3 Evaluate(in ShadedSurface surface, in ShadingVectors shadingVectors)
	{
		const float3x3 worldToTangent = WorldToTangentFrame(surface.normal);
		const float3 wi_local = mul(worldToTangent, shadingVectors.lightDir);
		const float3 wo_local = mul(worldToTangent, shadingVectors.viewDir);

		if (wi_local.z < brdf_utils::eon::DENOM_TOLERANCE || wo_local.z < brdf_utils::eon::DENOM_TOLERANCE)
		{
			return 0.f;
		}

		return brdf_utils::eon::f_EON(surface.albedo, surface.roughness, wi_local, wo_local, false);
	}

	static ShadingSample Sample(in float3 normal, in float roughness, in float3 viewDir, in float2 rng)
	{
#if EON_USE_EON_SAMPLING
		const float3x3 worldToTangent = WorldToTangentFrame(normal);
		const float3 wo_local = mul(worldToTangent, viewDir);

		if (wo_local.z < brdf_utils::eon::DENOM_TOLERANCE)
		{
			ShadingSample sample;
			sample.direction = 0.f;
			sample.pdf = 0.f;

			return sample;
		}

		const float4 wiP = brdf_utils::eon::sample_EON(wo_local, roughness, rng.x, rng.y);
		const float3 wi_local = wiP.xyz;
		const float pdf       = wiP.w;

		const float3x3 tangentToWorld = transpose(worldToTangent);

		ShadingSample sample;
		sample.direction = mul(tangentToWorld, wi_local);
		sample.pdf       = pdf;

		return sample;
#else
		const float3x3 tangentToWorld = TangentFrameToWorld(normal);

		ShadingSample sample;
		sample.direction = RandomVectorInCosineWeightedHemisphere(tangentToWorld, rng, OUT sample.pdf);

		return sample;
#endif // EON_USE_EON_SAMPLING
	}

	static float PDF(in float3 normal, in float roughness, in float3 viewDir, in float3 sampledDir)
	{
#if EON_USE_EON_SAMPLING
		const float3x3 worldToTangent = WorldToTangentFrame(normal);
		const float3 wo_local = mul(worldToTangent, viewDir);
		const float3 wi_local = mul(worldToTangent, sampledDir);

		if (wo_local.z < brdf_utils::eon::DENOM_TOLERANCE)
		{
			return 0.f;
		}

		return brdf_utils::eon::pdf_EON(wo_local, wi_local, roughness);
#else
		return PDFHemisphereCosineWeighted(normal, sampledDir);
#endif // EON_USE_EON_SAMPLING
	}
};

#endif // DIFFUSE_HLSLI
