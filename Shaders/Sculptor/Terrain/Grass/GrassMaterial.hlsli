#ifndef GRASS_MATERIAL_HLSLI
#define GRASS_MATERIAL_HLSLI

#include "Utils/Random.hlsli"


struct EvaluatedGrassMaterial
{
	float3 baseColor;
	float  roughness;
	float  visibility;
};


EvaluatedGrassMaterial EvaluateGrassMaterial(in float heightAlpha, in RngState bladeRng, in RngState clumpRng)
{
	EvaluatedGrassMaterial output;

	const float lushness       = saturate(bladeRng.Next() * 1.15f - 0.1f);
	const float dryness        = saturate(smoothstep(0.35f, 0.95f, clumpRng.Next()) + bladeRng.Next() * 0.15f - 0.05f);

	const float3 lushRootColor = lerp(float3(0.07f, 0.12f, 0.03f), float3(0.10f, 0.18f, 0.04f), lushness);
	const float3 lushTipColor  = lerp(float3(0.18f, 0.30f, 0.06f), float3(0.30f, 0.48f, 0.12f), lushness);
	const float3 dryRootColor  = lerp(float3(0.10f, 0.11f, 0.04f), float3(0.16f, 0.14f, 0.05f), lushness);
	const float3 dryTipColor   = lerp(float3(0.28f, 0.27f, 0.09f), float3(0.46f, 0.40f, 0.14f), lushness);

	const float rootDryness = dryness * 0.45f;
	const float tipDryness  = saturate(dryness * 0.85f + heightAlpha * 0.2f);
	const float3 rootColor  = lerp(lushRootColor, dryRootColor, rootDryness);
	const float3 tipColor   = lerp(lushTipColor, dryTipColor, tipDryness);

	output.baseColor  = lerp(rootColor, tipColor, heightAlpha);
	output.roughness  = saturate(lerp(0.88f, 0.72f, heightAlpha) + (bladeRng.Next() - 0.5f) * 0.08f + dryness * 0.06f) * 0.6f;
	output.visibility = (1.f - Pow2(heightAlpha)) * 0.8f + 0.2f;

	return output;
}

#endif // GRASS_MATERIAL_HLSLI
