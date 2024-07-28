#include "SculptorShader.hlsli"

[[descriptor_set(ReprojectVariableRateTextureDS, 0)]]

#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float2 SampleMotionTexture(in uint2 pixel)
{
	float2 motion = 0.f;
	float maxMotionSquared = 0.f;

	for(uint y = 0; y < 2; ++ y)
	{
		for(uint x = 0; x < 2; ++ x)
		{
			const float2 sampleMotion = u_motionTexture.Load(uint3(pixel * 2 + uint2(x, y), 0)).xy;
			const float sampleMotionSquared = dot(sampleMotion, sampleMotion);
			if(sampleMotionSquared > maxMotionSquared)
			{
				motion = sampleMotion;
				maxMotionSquared = sampleMotionSquared;
			}
		}
	}

	return motion;
}


[numthreads(8 , 4 , 1)]
void ReprojectVariableRateTextureCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	const float2 uv = (pixel + 0.5f) * u_constants.invResolution;

	const float2 motion = SampleMotionTexture(pixel);

	const uint variableRate = ReprojectVariableRate(u_inputTexture, uv, u_constants.resolution, motion);

	u_rwOutputTexture[pixel] = variableRate;
}
