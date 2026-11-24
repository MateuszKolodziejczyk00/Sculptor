#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]

[[shader_params(PresampleEmissivesConstants, CONSTS)]]

#include "Lights/StochasticDI/EmissiveSampling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void PresampleEmissivesCS(CS_INPUT input)
{
	const uint sampleIdx = input.globalID.x;

	RngState rngState = RngState::Create(uint2(sampleIdx, 0u), SCENE->gpuScene.frameIdx + 3917u);

	CONSTS->rwPresampledEmissives.Store(sampleIdx, GenerateEmissiveSample(rngState, CONSTS->emissivesSampler));
}
