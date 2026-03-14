#ifndef SPHERICAL_HARMONICS_HLSLI
#define SPHERICAL_HARMONICS_HLSLI

namespace SH2
{

float4 ComputeSHBasis(in float3 dir)
{
	float4 shBasis;
	shBasis.x = 0.28209479177387814f; // Y00
	shBasis.y = 0.4886025119029199f * dir.y; // Y1-1
	shBasis.z = 0.4886025119029199f * dir.z; // Y10
	shBasis.w = 0.4886025119029199f * dir.x; // Y11

	return shBasis;
}


float Evaluate(in float4 shCoeffs, in float3 dir)
{
	float4 shBasis = ComputeSHBasis(dir);
	return dot(shCoeffs, shBasis);
}

} // SH2


// Irradiance SH basis
// Based on https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc2018-precomputedgiobalilluminationinfrostbite.pdf
// Warning: It embeds 1 / PI from the BRDF
namespace ESH2
{

float4 ComputeSHBasis(in float3 dir)
{
	float4 shBasis;
	shBasis.x = 1.f;
	shBasis.y = 2.f * dir.x;
	shBasis.z = 2.f * dir.y;
	shBasis.w = 2.f * dir.z;

	return shBasis;
}


// Average remains unchanged, directional part is scaled based on that, so it's in [-1, 1] range
float4 Normalize(in float4 shCoeffs)
{
	const float averageL = shCoeffs.x;
	const float scale = averageL > 0.f ? rcp(averageL) : 1.f;
	return shCoeffs * float4(1.f, float3(scale, scale, scale));
}


float Average(in float4 shCoeffs)
{
	return shCoeffs.x;
}


float3 AverageDirection_NotNormalized(in float4 shCoeffs)
{
	return float3(shCoeffs.y, shCoeffs.z, shCoeffs.w);
}


float3 AverageDirection(in float4 shCoeffs)
{
	return normalize(AverageDirection_NotNormalized(shCoeffs));
}


float Evaluate(in float4 shCoeffs, in float3 dir)
{
	return (0.5f + dot(dir, AverageDirection_NotNormalized(shCoeffs))) * shCoeffs.x * 2.f;
}

} // ESH2

#endif // SPHERICAL_HARMONICS_HLSLI
