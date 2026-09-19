#include "SculptorShader.hlsli"

[[shader_params(TemporalAccumulationConstants, CONSTS)]]

#include "Utils/Sampling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void TemporalAccumulationCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	const float currentExposure = CONSTS->exposure[CONSTS->exposureOffset];

	const float3 historyColor = CONSTS->accumulatedData.Load(coords).xyz;

	const bool hasHistory = CONSTS->historyWeight > 0.f;

	float3 inputColor = CONSTS->input.Load(coords).xyz / currentExposure;
	if (any(isnan(inputColor)))
	{
		inputColor = hasHistory ? historyColor : 0.f;
	}

	const float3 accumulatedColor = inputColor * CONSTS->currentFrameWeight + historyColor * CONSTS->historyWeight;
	const float3 exposedColor = accumulatedColor * currentExposure;

	CONSTS->accumulatedData.Store(coords, float4(accumulatedColor, 1.f));
	CONSTS->output.Store(coords, float4(exposedColor, 1.f));
}
