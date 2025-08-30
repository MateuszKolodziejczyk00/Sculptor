#include "SculptorShader.hlsli"

[[bindless]]
[[descriptor_set(TemporalAccumulationDS, 1)]]

#include "Utils/Sampling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void TemporalAccumulationCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	const float currentExposure = u_exposure[u_constants.exposureOffset];

	const float3 inputColor = u_constants.input.Load(coords).xyz / currentExposure;

	const float3 historyColor = u_constants.accumulatedData.Load(coords).xyz;

	const float3 accumulatedColor = inputColor * u_constants.currentFrameWeight + historyColor * u_constants.historyWeight;
	const float3 exposedColor = accumulatedColor * currentExposure;

	u_constants.accumulatedData.Store(coords, float4(accumulatedColor, 1.f));
	u_constants.output.Store(coords, float4(exposedColor, 1.f));
}
