#ifndef CLOSURE_VIEWER_COMMON_HLSLI
#define CLOSURE_VIEWER_COMMON_HLSLI

// Requires ClosureViewerData bound as VIEWER

ShadedSurface CreateViewerSurface(in float3 normal)
{
	ShadedSurface surface;
	surface.normal             = normal;
	surface.roughness          = VIEWER->material.roughness;
	surface.clearCoatWeight    = VIEWER->material.clearCoatWeight;
	surface.clearCoatRoughness = VIEWER->material.clearCoatRoughness;
	ComputeSurfaceColor(VIEWER->material.baseColor, VIEWER->material.metallic, OUT surface.albedo, OUT surface.f0);

	// Delta BRDF is not allowed, as it can't be visualized as a lobe
	surface.roughness          = max(surface.roughness, OpaqueClosure::RoughnessDeltaThreshold);
	surface.clearCoatRoughness = max(surface.clearCoatRoughness, OpaqueClosure::RoughnessDeltaThreshold);

	return surface;
}

#endif // CLOSURE_VIEWER_COMMON_HLSLI
