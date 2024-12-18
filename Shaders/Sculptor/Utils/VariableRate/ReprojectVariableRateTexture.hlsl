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


uint GetReprojectionFailedVariableRate()
{
	if (u_constants.reprojectionFailedMode == 0)
	{
		return SPT_VARIABLE_RATE_1X1;
	}
	else if (u_constants.reprojectionFailedMode == 1)
	{
		return SPT_VARIABLE_RATE_2Y;
	}
	else if (u_constants.reprojectionFailedMode == 2)
	{
		return SPT_VARIABLE_RATE_2X2;
	}
#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	else if (u_constants.reprojectionFailedMode == 3)
	{
		return SPT_VARIABLE_RATE_4X4;
	}
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

	SPT_CHECK_MSG(false, L"Invalid reprojection failed mode");
	return SPT_VARIABLE_RATE_1X1;
}


[numthreads(8 , 4 , 1)]
void ReprojectVariableRateTextureCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	const float2 uv = (pixel + 0.5f) * u_constants.invResolution;

	const float2 motion = SampleMotionTexture(pixel);

	const VariableRateReprojectionResult reprojectionResult = ReprojectVariableRate(u_inputTexture, uv, u_constants.resolution, motion);
	const uint variableRate = reprojectionResult.isSuccessful ? reprojectionResult.variableRateMask : CreateCompressedVariableRateData(GetReprojectionFailedVariableRate());

	u_rwOutputTexture[pixel] = variableRate;

#if OUTPUT_REPROJECTION_SUCCESS_MASK

	const uint2 maskCoords = pixel / uint2(8u, 4u);

	uint successMask = reprojectionResult.isSuccessful ? 1u << WaveGetLaneIndex() : 0;
	successMask = WaveActiveBitOr(successMask);

	u_rwReprojectionSuccessMask[maskCoords] = successMask;

#endif // OUTPUT_REPROJECTION_SUCCESS_MASK
}
