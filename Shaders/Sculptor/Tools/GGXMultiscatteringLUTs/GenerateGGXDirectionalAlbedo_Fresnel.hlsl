#include "SculptorShader.hlsli"

[[shader_params(GenerateGGXDirectionalAlbedoFresnelConsts, CONSTS)]]

#include "Shading/Shading.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 4)]
void GenerateGGXDirectionalAlbedoFresnelCS(CS_INPUT input)
{
	const int3 coords = input.globalID;

	const uint3 lutSize = CONSTS->rwLUT.GetResolution();
	const float3 uv = (float3(coords) + 0.5f) / float3(lutSize);

	RngState rngState = RngState::Create(coords.xy + uint2(coords.z * lutSize.x, 0u), CONSTS->seed);

	const float NoV       = uv.x;
	const float roughness = uv.y;
	const float f0        = uv.z;

	const float3 normal = float3(0.f, 0.f, 1.f);
	const float3 viewDir = float3(sqrt(1.f - NoV * NoV), 0.f, NoV);

	ShadedSurface surface;
	surface.normal             = normal;
	surface.roughness          = roughness;
	surface.f0                 = f0;
	surface.albedo             = 0.f;
	surface.clearCoatWeight    = 0.f;
	surface.clearCoatRoughness = 0.f;

	float samplesSum = 0.f;

	const uint numSamples = 10000u;
	for (uint i = 0u; i < numSamples; ++i)
	{
		const float2 rnd = float2(rngState.Next(), rngState.Next());

		const ShadingSample sample = SpecularBRDF::Sample(normal, roughness, viewDir, rnd);

		const ShadingVectors shadingVectors = { viewDir, sample.direction, normal };

		if (shadingVectors.NoL > 0.f)
		{
			float3 specularThroughput;
			const float brdf = SpecularBRDF::Evaluate(surface, shadingVectors, OUT specularThroughput).x * shadingVectors.NoL;

			samplesSum += brdf / sample.pdf;
		}
	}

	const float E = samplesSum / float(numSamples);

	CONSTS->rwLUT[coords] = E;
}
