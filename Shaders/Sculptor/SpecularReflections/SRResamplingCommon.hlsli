#ifndef SRRESAMPLING_COMMON_HLSLI
#define SRRESAMPLING_COMMON_HLSLI

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/Shapes.hlsli"
#include "Utils/Packing.hlsli"


struct MinimalSurfaceInfo
{
	float3 location;
	float3 n;
	float  roughness;
	float  clearCoatWeight;
	float  clearCoatRoughness;
	float3 v;
	half3  f0;
	half3  diffuseColor;
};


struct MinimalGBuffer
{
	SRVTexture2D<float>  depthTexture;
	SRVTexture2D<float2> normalsTexture;
	SRVTexture2D<float4> baseColorMetallicTexture;
	SRVTexture2D<float4> propsTexture;
};


MinimalSurfaceInfo GetMinimalSurfaceInfo(in MinimalGBuffer minimalGBuffer, in uint2 pixel, in float2 pixelSize, in SceneViewData sceneView)
{
	const float depth = minimalGBuffer.depthTexture.Load(uint3(pixel, 0));
	const float2 uv = (pixel + 0.5f) * pixelSize;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 sampleLocation = NDCToWorldSpace(ndc, sceneView);
	const float3 sampleNormal = OctahedronDecodeNormal(minimalGBuffer.normalsTexture.Load(uint3(pixel, 0)));
	const float3 toView = normalize(sceneView.viewLocation - sampleLocation);

	const float4 props = minimalGBuffer.propsTexture.Load(uint3(pixel, 0));
	const float4 baseColorMetallic = minimalGBuffer.baseColorMetallicTexture.Load(uint3(pixel, 0));
	float3 f0;
	float3 diffuseColor;
	ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT f0);

	MinimalSurfaceInfo pixelSurface;
	pixelSurface.location           = sampleLocation;
	pixelSurface.n                  = sampleNormal;
	pixelSurface.v                  = toView;
	pixelSurface.f0                 = half3(f0);
	pixelSurface.diffuseColor       = half3(diffuseColor);
	pixelSurface.roughness          = props.x;
	pixelSurface.clearCoatWeight    = props.y;
	pixelSurface.clearCoatRoughness = props.z;

	return pixelSurface;
}


float EvaluateTargetFunction(in MinimalSurfaceInfo surface, in float3 sampleLocation, in float3 sampleLuminance)
{
	const float3 l = normalize(sampleLocation - surface.location);

	const float dotNL = saturate(dot(surface.n, l));

	ShadedSurface shadedSurface;
	shadedSurface.normal             = surface.n;
	shadedSurface.f0                 = float3(surface.f0);
	shadedSurface.albedo             = float3(surface.diffuseColor);
	shadedSurface.roughness          = surface.roughness;
	shadedSurface.clearCoatWeight    = surface.clearCoatWeight;
	shadedSurface.clearCoatRoughness = surface.clearCoatRoughness;

	const BRDFLobes brdf = RTClosure::RTEvaluate(shadedSurface, surface.v, l);
	if(any(isnan(brdf.specular)))
	{
		return 0.f;
	}

	const float3 Lo = sampleLuminance * (brdf.specular + brdf.diffuse);

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
