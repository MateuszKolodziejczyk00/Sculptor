#include "SculptorShader.hlsli"

[[descriptor_set(GenerateRayDirectionsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/Random.hlsli"

#include "SpecularReflections\SpecularReflectionsCommon.hlsli"

#include "Shading/Shading.hlsli"

#define USE_BLUE_NOISE_SAMPLES 0

#if USE_BLUE_NOISE_SAMPLES
#include "Utils/BlueNoiseSamples.hlsli"
#endif // USE_BLUE_NOISE_SAMPLES

struct RayDirectionInfo
{
	float3 direction;
	float  pdf;
};


RayDirectionInfo GenerateReflectionRayDir(in float3 normal, in float roughness, in float3 toView, in uint2 pixel)
{
	RayDirectionInfo result;

	float3 h;
	if(roughness < SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		result.pdf       = 1.f;
		result.direction = reflect(-toView, normal);
	}
	else
	{
#if USE_BLUE_NOISE_SAMPLES
		const uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x & 15u) + u_constants.frameIdx * 23u)) & 255u;
#endif // USE_BLUE_NOISE_SAMPLES

		uint attempt = 0;
		while(true)
		{
#if USE_BLUE_NOISE_SAMPLES
			const uint currentSampleIdx = (sampleIdx + attempt) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[currentSampleIdx] + u_constants.random);
#else
			RngState rng = RngState::Create(pixel, u_constants.frameIdx * 3);
			const float2 noise = float2(rng.Next(), rng.Next());
#endif // USE_BLUE_NOISE_SAMPLES

			const float3 h = SampleVNDFIsotropic(noise, toView, RoughnessToAlpha(roughness), normal);

			const float3 reflectedRay = reflect(-toView, h);

			if(dot(reflectedRay, normal) > 0.f || attempt == 16u) 
			{
				result.pdf       = PDFVNDFIsotrpic(toView, reflectedRay, RoughnessToAlpha(roughness), normal);
				result.direction = reflectedRay;

				 break;
			}

			++attempt;
		}
	}

	return result;
}


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 4, 1)]
void GenerateRayDirectionsCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * u_constants.invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 normal   = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);

		const RayDirectionInfo rayDirection = GenerateReflectionRayDir(normal, roughness, toView, pixel);

		u_rwRaysDirectionTexture[pixel] = OctahedronEncodeNormal(rayDirection.direction);
		u_rwRaysPdfTexture[pixel]       = rayDirection.pdf;
	}
}
