#include "SculptorShader.hlsli"

[[shader_params(ClosureViewerData, VIEWER)]]
[[shader_params(ClosureFurnanceTestConstants, CONSTS)]]

#include "Utils/Shapes.hlsli"
#include "Utils/Random.hlsli"
#include "Shading/Shading.hlsli"
#include "Tools/ClosureViewer/ClosureViewerCommon.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ClosureFurnanceTestCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const uint2 outputRes = CONSTS->rwOutput.GetResolution();

	if (any(coords >= outputRes))
	{
		return;
	}

	const float2 screenOffset = (float2(coords) + 0.5f - float2(outputRes) * 0.5f) / float(min(outputRes.x, outputRes.y));

	const float3 cameraOrigin = -FORWARD_VECTOR * 3.f;
	const float3 cameraDirection = normalize(FORWARD_VECTOR + screenOffset.x * RIGHT_VECTOR - screenOffset.y * UP_VECTOR);

	const Ray cameraRay = Ray::Create(cameraOrigin, cameraDirection);

	const Sphere sphere = Sphere::Create(float3(0.f, 0.f, 0.f), 1.f);

	const IntersectionResult intersection = cameraRay.IntersectSphere(sphere);

	RngState rng = RngState::Create(coords, VIEWER->frameIdx);

	const float3 background = 0.8f;

	if (intersection.IsValid())
	{
		const float3 intersectionPoint = cameraRay.origin + cameraRay.direction * intersection.GetTime();
		const float3 normal = normalize(intersectionPoint - sphere.center);
		const float3 viewDir = -cameraDirection;

		const ShadedSurface surface = CreateViewerSurface(normal);

		const float rcpSamplesNum = 1.f / float(CONSTS->samplesNum);

		float3 Lo = 0.f;

		for (uint i = 0u; i < CONSTS->samplesNum; ++i)
		{
			const ShadingSample sample = OpaqueClosure::Sample(surface, viewDir, rng);

			const BRDFLobes lobes = OpaqueClosure::Evaluate<true>(surface, viewDir, sample.direction);

			const float3 sampleLo = background * (lobes.diffuse + lobes.specular) / sample.pdf;

			Lo += sampleLo * rcpSamplesNum;
		}

		const float3 prevLo = VIEWER->frameIdx > 0u ? CONSTS->rwOutput[coords].xyz : 0.f;
		const float3 newLo = (prevLo * VIEWER->frameIdx + Lo) / (VIEWER->frameIdx + 1.f);
		CONSTS->rwOutput[coords] = float4(newLo, 1.f);
	}
	else
	{
		CONSTS->rwOutput[coords] = float4(background, 1.f);
	}
}
