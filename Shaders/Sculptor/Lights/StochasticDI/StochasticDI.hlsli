#ifndef STOCHASTIC_DI_HLSLI
#define STOCHASTIC_DI_HLSLI

#include "SpecularReflections/RTGICommon.hlsli"
#include "Utils/MortonCode.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"


[[shader_struct(DIPackedReservoir)]]


struct EmissiveSample
{
	float3 location;
	float3 normal;
	float3 luminance;
	float pdf_A;
};


struct DIReservoir
{
	EmissiveSample sample;
	uint16_t   M;
	uint16_t   age;
	float      weightSum;
	float      selected_p_hat;

	static DIReservoir Create()
	{
		DIReservoir reservoir;
		reservoir.sample.location  = 0.f;
		reservoir.sample.normal    = 0.f;
		reservoir.sample.luminance = 0.f;
		reservoir.sample.pdf_A     = 0.f;
		reservoir.M                = 0u;
		reservoir.age              = 0u;
		reservoir.weightSum        = 0.f;
		reservoir.selected_p_hat   = 0.f;

		return reservoir;
	}

	bool IsValid()
	{
		return selected_p_hat > 0.f;
	}

	bool Update(in float rnd, in EmissiveSample newSample, in float p_hat)
	{
		const float w_i = p_hat / newSample.pdf_A;

		weightSum += w_i;
		M         += 1u;

		const bool updateReservoir = (rnd * weightSum < w_i);

		if(updateReservoir)
		{
			sample         = newSample;
			selected_p_hat = p_hat;
		}

		return updateReservoir;
	}

	bool Update(in float rnd, in DIReservoir other, in float p_hat)
	{
		const float w_i = other.M * p_hat * other.weightSum;

		weightSum += w_i;
		M         += other.M;

		const bool updateReservoir = (rnd * weightSum < w_i);

		if(updateReservoir)
		{
			sample         = other.sample;
			selected_p_hat = p_hat;
		}

		return updateReservoir;
	}

	void Normalize()
	{
		weightSum = selected_p_hat > 0.f ? (1.f / selected_p_hat) * weightSum * (1.f / M) : 0.f;
	}
};


uint GetScreenReservoirIdx(in uint2 coords, in uint2 resolution)
{
	const uint2 tileSize = uint2(64u, 64u);
	const uint2 tilesRes = resolution >> 6;
	const uint2 tileCoords = coords >> 6;

	const uint tileIdx = tileCoords.y * tilesRes.x + tileCoords.x;

	const uint2 coordsInTile = coords & 63u;
	const uint reservoirIdxInTile = EncodeMorton2(coordsInTile);

	return tileIdx * Pow2(64u) + reservoirIdxInTile;
}


DIPackedReservoir PackDIReservoir(in DIReservoir reservoir)
{
	DIPackedReservoir packed;
	packed.location          = reservoir.sample.location;
	packed.packedLuminance   = EncodeRGBToLogLuv(reservoir.sample.luminance);
	packed.hitNormal         = OctahedronEncodeNormal(reservoir.sample.normal);
	packed.weight            = reservoir.weightSum;
	packed.MAndProps         = (min(reservoir.M, 20u) & 0xFFu) | (reservoir.age << 8u);

	return packed;
}


DIReservoir UnpackDIReservoir(in DIPackedReservoir packedReservoir)
{
	DIReservoir reservoir;
	reservoir.sample.location  = packedReservoir.location;
	reservoir.sample.luminance = DecodeRGBFromLogLuv(packedReservoir.packedLuminance);
	reservoir.sample.normal    = OctahedronDecodeNormal(packedReservoir.hitNormal);
	reservoir.weightSum        = packedReservoir.weight;
	reservoir.M                = uint16_t(packedReservoir.MAndProps & 0xFFu);
	reservoir.age              = uint16_t((packedReservoir.MAndProps >> 8u) & 0xFFu);
	reservoir.selected_p_hat   = reservoir.weightSum > 0.f ? reservoir.weightSum / reservoir.M : 0.f;

	return reservoir;
}


/*
 * Samples are generated uniformly on the area of the emissive geometry.
 * This means that pdf is in area measure. When we evaluate sample contribution, this needs to be converted to solid angle measure
 * Based on https://pbr-book.org/4ed/Monte_Carlo_Integration/Transforming_between_Distributions, in order to do this conversion, we need to multiply area pdf by inverse of dw/dA
 * dw/dA = cos(theta) / r^2 (as described in https://pbr-book.org/4ed/Radiometry,_Spectra,_and_Color/Working_with_Radiometric_Integrals)
 * therefore:
 *
 * pdf_w = pdf_A * (dw/dA)^(-1) = pdf_A * r^2 / cos(theta)
 *
 * With such sample distribution Light Transport Equation requires additional Geometric term which is defined in PBRT in https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/The_Light_Transport_Equation as jacobian,
 * that relates solid angle to area measure therefore it's based on dw/dA
 *
 * G = dw/dA = cos(theta) / r^2
 *
 * In case of PBRT, Geometry term additionally include visiblity term V which is necessary when samples are generated on the area of the light source,
 * but in our case, it's skipped to be able to compute unshadowed path contribution. Additionally, we also don't include NoL in the geometric term
 * So essentially, GeometricTerm should be the same as the one used in Bitterli paper: https://research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf
 */
float AreaToSolidAnglePDF(float pdf_A, float3 sampleNormal, float3 toSample)
{
	const float cosTheta = dot(sampleNormal, normalize(-toSample));
	const float r2 = dot(toSample, toSample);
	return cosTheta > 0.f ? pdf_A * r2 / cosTheta : 0.f;
}


float GeometricTermNoVisibility(float3 sampleNormal, float3 L, float r2)
{
	const float cosTheta = dot(sampleNormal, -L);
	return max(cosTheta / r2, 0.f);
}


float3 UnshadowedPathContribution(in SurfaceInfo surface, in float3 V, in EmissiveSample sample)
{
	float3 Li = 0.f;

	const float3 toSample = sample.location - surface.location;
	const float r2 = dot(toSample, toSample);
	const float3 L = toSample / sqrt(r2);

	const float G = GeometricTermNoVisibility(sample.normal, L, r2);

	if (G < 1e-6f)
	{
		return Li;
	}

	const float NdotL = max(dot(surface.normal, L), 0.f);

	const RTBRDF brdf = RT_EvaluateBRDF(surface.normal, V, L, surface.roughness, surface.specularColor, surface.diffuseColor);
	Li = sample.luminance * (brdf.diffuse + brdf.specular) * NdotL * G;

	return Li;
}

#endif // STOCHASTIC_DI_HLSLI
