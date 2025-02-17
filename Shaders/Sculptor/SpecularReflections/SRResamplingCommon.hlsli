#ifndef SRRESAMPLING_COMMON_HLSLI
#define SRRESAMPLING_COMMON_HLSLI

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/Shapes.hlsli"
#include "Utils/Packing.hlsli"


struct MinimalSurfaceInfo
{
	float3 location;
	float3 n;
	float  roughness;
	float3 v;
	half3  f0;
	half3  diffuseColor;
};


struct MinimalGBuffer
{
	Texture2D<float>  depthTexture;
	Texture2D<float2> normalsTexture;
	Texture2D<float4> baseColorMetallicTexture;
	Texture2D<float>  roughnessTexture;
};


MinimalSurfaceInfo GetMinimalSurfaceInfo(in MinimalGBuffer minimalGBuffer, in uint2 pixel, in SceneViewData sceneView)
{
	const float depth = minimalGBuffer.depthTexture.Load(uint3(pixel, 0));
	const float2 uv = (pixel + 0.5f) * u_resamplingConstants.pixelSize;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 sampleLocation = NDCToWorldSpace(ndc, sceneView);
	const float3 sampleNormal = OctahedronDecodeNormal(minimalGBuffer.normalsTexture.Load(uint3(pixel, 0)));
	const float3 toView = normalize(sceneView.viewLocation - sampleLocation);

	const float roughness = minimalGBuffer.roughnessTexture.Load(uint3(pixel, 0));
	const float4 baseColorMetallic = minimalGBuffer.baseColorMetallicTexture.Load(uint3(pixel, 0));
	float3 f0;
	float3 diffuseColor;
	ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT f0);

	MinimalSurfaceInfo pixelSurface;
	pixelSurface.location     = sampleLocation;
	pixelSurface.n            = sampleNormal;
	pixelSurface.v            = toView;
	pixelSurface.f0           = half3(f0);
	pixelSurface.diffuseColor = half3(diffuseColor);
	pixelSurface.roughness    = roughness;

	return pixelSurface;
}


float EvaluateTargetFunction(in MinimalSurfaceInfo surface, in float3 sampleLocation, in float3 sampleLuminance)
{
	const float3 l = normalize(sampleLocation - surface.location);

	const float dotNL = saturate(dot(surface.n, l));

	const float3 specular = SR_GGX_Specular(surface.n, surface.v, l, max(surface.roughness, 0.05f), surface.f0);
	if(any(isnan(specular)))
	{
		return 0.f;
	}

	const float3 Lo = dotNL * sampleLuminance * (specular + Diffuse_Lambert(surface.diffuseColor));

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

	jacobian = clamp(jacobian, 0.2f, 3.f);

	return jacobian;
}


bool SurfacesAllowResampling(in MinimalSurfaceInfo surfaceA, in MinimalSurfaceInfo surfaceB)
{
	const float planeDistanceThreshold = 0.045f;
	const float normalThreshold = 0.4f;

	const Plane surfacePlane = Plane::Create(surfaceA.n, surfaceA.location);
	const float distance = surfacePlane.Distance(surfaceB.location);
	return distance <= planeDistanceThreshold
		&& dot(surfaceA.n, surfaceB.n) > normalThreshold;
}


bool MaterialsAllowResampling(in MinimalSurfaceInfo surfaceA, in MinimalSurfaceInfo surfaceB)
{
	const float f0Threshold = 0.2f;
	const float roughnessThreshold = 0.5f;

	const float f0A = Luminance(surfaceA.f0);
	const float f0B = Luminance(surfaceB.f0);

	const float diffuseA = Luminance(surfaceA.diffuseColor);
	const float diffuseB = Luminance(surfaceB.diffuseColor);

	return AreNearlyEqual(f0A, f0B, f0Threshold)
		&& AreNearlyEqual(diffuseA, diffuseB, f0Threshold)
		&& AreNearlyEqual(surfaceA.roughness, surfaceB.roughness, roughnessThreshold);
}

bool IsReservoirValidForSurface(in SRReservoir reservoir, in MinimalSurfaceInfo surface)
{
	return reservoir.IsValid() && dot((reservoir.hitLocation - surface.location), surface.n) > 0.f;
}

#endif // SRRESAMPLING_COMMON_HLSLI