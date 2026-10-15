#include "SculptorShader.hlsli"

[[shader_params(GenerateGGXAverageEnergyFresnelConsts, CONSTS)]]

#include "Shading/Shading.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void GenerateGGXAverageEnergy_FresnelCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	const uint2 lutSize = CONSTS->rwLUT.GetResolution();
	const float2 uv = (float2(coords) + 0.5f) / float2(lutSize);

	const float roughness = uv.x;
	const float f0        = uv.y;

	const float3 normal = float3(0.f, 0.f, 1.f);

	RngState rngState = RngState::Create(coords, CONSTS->seed);

	float samplesSum = 0.f;

	const uint3 directionalAlbedoResolution = CONSTS->directionalAlbedoLUT.GetResolution();

	const uint numSamples = directionalAlbedoResolution.x;
	for (uint i = 0u; i < numSamples; ++i)
	{
		const float NoV = (i + 0.5f) / float(numSamples);

		const float3 directionalAlbedoUVW = float3(NoV, roughness, f0);

		const float directionalAlbedo = CONSTS->directionalAlbedoLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), directionalAlbedoUVW, 0.f);

		samplesSum += directionalAlbedo * NoV;
	}

	const float Eavg = 2.f * samplesSum / float(numSamples);

	CONSTS->rwLUT[coords] = Eavg;
}
