#ifndef SRRESAMPLING_COMMON_HLSLI
#define SRRESAMPLING_COMMON_HLSLI

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "Utils/Shapes.hlsli"


struct MinimalSurfaceInfo
{
	float3 location;
	float3 n;
	float3 v;
	float3 f0;
	float  roughness;
};


float EvaluateTargetPdf(in MinimalSurfaceInfo surface, in float3 sampleLocation, in float3 sampleLuminance)
{
	const float3 l = normalize(sampleLocation - surface.location);

	const float3 specular = SR_GGX_Specular(surface.n, surface.v, l, surface.roughness, surface.f0);

	const float dotNL = saturate(dot(surface.n, l));

	const float3 Lo = saturate(dotNL) * specular * sampleLuminance;

	return Luminance(Lo);
}


float EvaluateJacobian(in float3 destinationSampleLocation, in float3 reuseSampleLocation, in SRReservoir reuseReservoir)
{
	float3 toReuse = reuseSampleLocation - reuseReservoir.hitLocation;
	const float reuseDistance = length(toReuse);
	toReuse = toReuse / reuseDistance;

	float3 toDest = destinationSampleLocation - reuseReservoir.hitLocation;
	const float destDistance = length(toDest);
	toDest = toDest / destDistance;

	const float cosPhiReuse = abs(dot(reuseReservoir.hitNormal, toReuse));
	const float cosPhiDest = abs(dot(reuseReservoir.hitNormal, toDest));

	// Equation (11) from Restir GI paper
	float jacobian = (cosPhiReuse / cosPhiDest) * (Pow2(destDistance) / Pow2(reuseDistance));

	if(isnan(jacobian) || isinf(jacobian))
	{
		jacobian = 0.f;
	}

	return jacobian;
}


bool ValidateJabobian(inout float jacobian)
{
	if(jacobian > 10.f || jacobian < 0.1f)
	{
		return false;
	}

	jacobian = clamp(jacobian, 0.3333f, 3.f);
	return true;
}


bool AreSurfacesSimilar(in MinimalSurfaceInfo surfaceA, in MinimalSurfaceInfo surfaceB)
{
	const Plane surfacePlane = Plane::Create(surfaceA.n, surfaceA.location);
	const float distance = surfacePlane.Distance(surfaceB.location);
	return distance <= 0.05f
		&& dot(surfaceA.n, surfaceB.n) > 0.95f;
}


bool AreMaterialsSimilar(in MinimalSurfaceInfo surfaceA, in MinimalSurfaceInfo surfaceB)
{
	const float f0A = Luminance(surfaceA.f0);
	const float f0B = Luminance(surfaceB.f0);

	return AreNearlyEqual(f0A, f0B, 0.1f)
		&& AreNearlyEqual(surfaceA.roughness, surfaceB.roughness, 0.1f);
}

#endif // SRRESAMPLING_COMMON_HLSLI