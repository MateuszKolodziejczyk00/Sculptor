#ifndef SRRESAMPLING_COMMON_HLSLI
#define SRRESAMPLING_COMMON_HLSLI

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/Shapes.hlsli"


struct MinimalSurfaceInfo
{
	float3 location;
	float3 n;
	float3 v;
	float3 f0;
	float  roughness;
};


struct MinimalGBuffer
{
	Texture2D<float>  depthTexture;
	Texture2D<float3> normalsTexture;
	Texture2D<float3> specularColorTexture;
	Texture2D<float>  roughnessTexture;
};


MinimalSurfaceInfo GetMinimalSurfaceInfo(in MinimalGBuffer minimalGBuffer, in uint2 pixel, in SceneViewData sceneView)
{
	const float depth = minimalGBuffer.depthTexture.Load(uint3(pixel, 0));
	const float2 uv = (pixel + 0.5f) * u_resamplingConstants.pixelSize;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 sampleLocation = NDCToWorldSpace(ndc, sceneView);
	const float3 sampleNormal = minimalGBuffer.normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;
	const float3 toView = normalize(sceneView.viewLocation - sampleLocation);

	const float roughness = minimalGBuffer.roughnessTexture.Load(uint3(pixel, 0));
	const float3 f0 = minimalGBuffer.specularColorTexture.Load(uint3(pixel, 0)).rgb;

	MinimalSurfaceInfo pixelSurface;
	pixelSurface.location  = sampleLocation;
	pixelSurface.n         = sampleNormal;
	pixelSurface.v         = toView;
	pixelSurface.f0        = f0;
	pixelSurface.roughness = roughness;

	return pixelSurface;
}


float EvaluateTargetPdf(in MinimalSurfaceInfo surface, in float3 sampleLocation, in float3 sampleLuminance)
{
	const float3 l = normalize(sampleLocation - surface.location);

	const float3 specular = SR_GGX_Specular(surface.n, surface.v, l, surface.roughness, surface.f0);

	const float dotNL = saturate(dot(surface.n, l));

	const float3 Lo = dotNL * specular * sampleLuminance;

	return Luminance(Lo);
}


float EvaluateJacobian(in float3 destinationSampleLocation, in float3 reuseSampleLocation, in SRReservoir reuseReservoir)
{
	if (reuseReservoir.HasFlag(SR_RESERVOIR_FLAGS_MISS))
	{
		return 1.f;
	}

	float3 toReuse = reuseSampleLocation - reuseReservoir.hitLocation;
	const float reuseDistance = length(toReuse);
	toReuse = toReuse / reuseDistance;

	float3 toDest = destinationSampleLocation - reuseReservoir.hitLocation;
	const float destDistance = length(toDest);
	toDest = toDest / destDistance;

	const float cosPhiReuse = abs(dot(reuseReservoir.hitNormal, toReuse));
	const float cosPhiDest = abs(dot(reuseReservoir.hitNormal, toDest));

	// Equation (11) from Restir GI paper
	float jacobian = (cosPhiDest / cosPhiReuse) * Pow2(reuseDistance / destDistance);

	if(isnan(jacobian) || isinf(jacobian))
	{
		jacobian = -1.f;	
	}

	if(jacobian > 10.f || jacobian < 0.1f)
	{
		jacobian = -1.f;
	}

	jacobian = clamp(jacobian, 0.3333f, 3.f);

	return jacobian;
}


bool AreSurfacesSimilar(in MinimalSurfaceInfo surfaceA, in MinimalSurfaceInfo surfaceB)
{
	const float planeDistanceThreshold = 0.07f;
	const float normalThreshold = 0.6f;

	const Plane surfacePlane = Plane::Create(surfaceA.n, surfaceA.location);
	const float distance = surfacePlane.Distance(surfaceB.location);
	return distance <= planeDistanceThreshold
		&& dot(surfaceA.n, surfaceB.n) > normalThreshold;
}


bool AreMaterialsSimilar(in MinimalSurfaceInfo surfaceA, in MinimalSurfaceInfo surfaceB)
{
	const float f0Threshold = 0.25f;
	const float roughnessThreshold = 0.5f;

	const float f0A = Luminance(surfaceA.f0);
	const float f0B = Luminance(surfaceB.f0);
	return AreNearlyEqual(f0A, f0B, f0Threshold)
		&& AreNearlyEqual(surfaceA.roughness, surfaceB.roughness, roughnessThreshold);
}

bool IsReservoirValidForSurface(in SRReservoir reservoir, in MinimalSurfaceInfo surface)
{
	return reservoir.IsValid() && dot((reservoir.hitLocation - surface.location), surface.n) > 0.f;
}

#endif // SRRESAMPLING_COMMON_HLSLI