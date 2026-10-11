#include "SculptorShader.hlsli"

[[shader_params(ClosureViewerData, VIEWER)]]
[[shader_params(ClosureGraphConstants, CONSTS)]]

#include "Shading/Shading.hlsli"
#include "Tools/ClosureViewer/ClosureViewerCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


// Plot shows plane of incidence (x - horizontal, z - up), origin is shifted down to make more space for the upper hemisphere
static const float plotExtent        = 2.5f;
static const float plotOriginOffset  = 0.3f;
static const float gridCellSize      = 0.25f;
static const float directionLength   = 1.5f;


float DistanceToSegment(in float2 p, in float2 a, in float2 b)
{
	const float2 pa = p - a;
	const float2 ba = b - a;
	const float t = saturate(dot(pa, ba) / dot(ba, ba));
	return length(pa - ba * t);
}


float LineCoverage(in float distance, in float pixelSize, in float widthInPixels)
{
	return 1.f - saturate(distance / pixelSize - widthInPixels * 0.5f);
}


[numthreads(8, 8, 1)]
void ClosureGraphCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const uint2 outputRes = CONSTS->rwOutput.GetResolution();

	if (any(coords >= outputRes))
	{
		return;
	}

	const float minDimension = float(min(outputRes.x, outputRes.y));
	const float2 screenOffset = (float2(coords) + 0.5f - float2(outputRes) * 0.5f) / minDimension;

	const float pixelSize = plotExtent / minDimension;

	const float2 p = float2(-screenOffset.x, plotOriginOffset - screenOffset.y) * plotExtent;
	const float r = length(p);

	// Background with polar grid
	float3 color = p.y >= 0.f ? 0.08f : 0.04f;

	const float circleDist = abs(frac(r / gridCellSize + 0.5f) - 0.5f) * gridCellSize;
	color = lerp(color, 0.2f, LineCoverage(circleDist, pixelSize, 1.f) * (p.y >= 0.f ? 1.f : 0.4f));

	color = lerp(color, 0.5f, LineCoverage(abs(p.y), pixelSize, 1.5f));

	// Closure
	const float3 N = UP_VECTOR;
	const float3 V = VIEWER->viewDirection;

	if (r > 0.f)
	{
		const float3 L = float3(p.x, 0.f, p.y) / r;

		const ShadedSurface surface = CreateViewerSurface(N);
		const BRDFLobes lobes = OpaqueClosure::Evaluate<false>(surface, V, L);

		const float diffuseLum  = Luminance(lobes.diffuse);
		const float specularLum = Luminance(lobes.specular);
		const float totalLum    = diffuseLum + specularLum;

		const float diffuseRadius = diffuseLum * VIEWER->closureScale;
		const float totalRadius   = totalLum * VIEWER->closureScale;

		const float3 diffuseColor  = float3(0.2f, 0.55f, 1.f);
		const float3 specularColor = float3(1.f, 0.55f, 0.15f);

		if (r < totalRadius)
		{
			const float3 lobeColor = r < diffuseRadius ? diffuseColor : specularColor;
			color = lerp(color, lobeColor, VIEWER->closureOpacity);
		}

		// Radial distance is only approximation of distance to curve, but it's good enough for outlines
		color = lerp(color, diffuseColor, LineCoverage(abs(r - diffuseRadius), pixelSize, 1.5f) * (diffuseRadius > 0.f ? 1.f : 0.f));
		color = lerp(color, 1.f, LineCoverage(abs(r - totalRadius), pixelSize, 1.5f) * (totalRadius > 0.f ? 1.f : 0.f));
	}

	// Directions
	const float3 mirrorL = reflect(-V, N);

	color = lerp(color, float3(0.3f, 0.6f, 1.f), LineCoverage(DistanceToSegment(p, 0.f, N.xz * directionLength), pixelSize, 2.f));
	color = lerp(color, float3(1.f, 0.3f, 0.3f), LineCoverage(DistanceToSegment(p, 0.f, mirrorL.xz * directionLength), pixelSize, 2.f));
	color = lerp(color, float3(1.f, 0.85f, 0.1f), LineCoverage(DistanceToSegment(p, 0.f, V.xz * directionLength), pixelSize, 2.f));

	CONSTS->rwOutput[coords] = float4(color, 1.f);
}
