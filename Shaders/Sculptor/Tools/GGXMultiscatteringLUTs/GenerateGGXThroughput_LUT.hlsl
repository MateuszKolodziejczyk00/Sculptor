#include "SculptorShader.hlsli"

[[shader_params(GenerateGGXThroughputLUTConsts, CONSTS)]]

#include "Shading/Shading.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 4)]
void GenerateGGXThroughput_LUTCS(CS_INPUT input)
{
	const int3 coords = input.globalID;

	const uint3 lutSize = CONSTS->rwLUT.GetResolution();
	const float3 uv = (float3(coords) + 0.5f) / float3(lutSize);

	RngState rngState = RngState::Create(coords.xy + uint2(coords.z * lutSize.x, 0u), CONSTS->seed);

	const float NoV       = uv.x;
	const float roughness = uv.y;
	const float f0        = uv.z;

	const float directionalAlbedo = CONSTS->directionalAlbedoLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float3(NoV, roughness, f0), 0.f);

	const float E = CONSTS->averageEnergyLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(roughness, f0), 0.f);

	const float throughput     = (1.f - directionalAlbedo) / sqrt(1.f - E);
	const float coatThroughput = (1.f - directionalAlbedo);

	CONSTS->rwLUT[coords] = throughput;
}
