#include "SculptorShader.hlsli"

[[shader_params(GenerateRoughnessViewLUTConsts, CONSTS)]]

#include "Shading/Shading.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void GenerateRoughnessViewLUTCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	RngState rngState = RngState::Create(coords, CONSTS->seed);

	const uint2 lutSize = CONSTS->rwLUT.GetResolution();
	const float2 uv = (float2(coords) + 0.5f) / float2(lutSize);

	const float roughness = uv.y;
	const float NoV       = uv.x;

	const float3 normal = float3(0.f, 0.f, 1.f);
	const float3 viewDir = float3(sqrt(1.f - NoV * NoV), 0.f, NoV);

	const float alpha = RoughnessToAlpha(roughness);

	float samplesSum = 0.f;

	const uint numSamples = 10000u;
	for (uint i = 0u; i < numSamples; ++i)
	{
		const float2 rnd = float2(rngState.Next(), rngState.Next());

		const float3 h = brdf_utils::vndf_isotropic::Sample(rnd, viewDir, alpha, normal);
		const float3 lightDir = reflect(-viewDir, h);

		const float a2 = alpha * alpha;
		const float NoL = dot(normal, lightDir);
		const float NoV = dot(normal, viewDir);
		const float NoH = dot(normal, h);

		if (NoL > 0.f)
		{
			const float sample = brdf_utils::V_SmithGGXCorrelated(a2, NoL, NoV) / brdf_utils::GGX_V1(a2, NoV) * (2.f * NoL);

			samplesSum += sample;
		}
	}

	const float E = samplesSum / float(numSamples);

	CONSTS->rwLUT[coords] = 1.f - E;
}
