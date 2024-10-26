#include "SculptorShader.hlsli"

[[descriptor_set(RTVariableRateTextureDS, 1)]]

#define VR_BUILDER_SINGLE_LANE_PER_QUAD 1

#include "Utils/VariableRate/VariableRateBuilder.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};


struct RTVariableRateCallback
{
	static uint ComputeVariableRateMask(in VariableRateProcessor processor)
	{
		if(u_rtConstants.forceFullRateTracing)
		{
			return SPT_VARIABLE_RATE_1X1;
		}

		const float2 uv = (processor.GetCoords() + 1u) / float2(u_constants.inputResolution);

		const float4 linearDepth2x2 = u_linearDepthTexture.GatherRed(u_nearestSampler, uv);
		const bool4  isDepthValid = linearDepth2x2 > 0.f;
		const bool anyDepthValid = any(isDepthValid);

		const float4 validDepthMask = select(isDepthValid, 1.f, 0.f);

		if(!anyDepthValid)
		{
			return SPT_VARIABLE_RATE_1X1;
		}

		const float rtReflectionsInfluence = MaxComponent(u_influenceTexture.GatherRed(u_nearestSampler, uv) * validDepthMask);
 
		// Roughness is in [0, 1] range. After adding (1 - validDepthMask) we get values above 1 for invalid pixels.
		const float roughness = -MinComponent(u_roughnessTexture.GatherRed(u_nearestSampler, uv) + -validDepthMask);

		uint variableRate = SPT_VARIABLE_RATE_4X4;

		if(roughness < 0.015f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_1X1);
		}

		if(variableRate != SPT_VARIABLE_RATE_1X1)
		{
			const float brightness = -MinComponent(u_reflectionsTexture.GatherRed(u_nearestSampler, uv) * -validDepthMask);

			const float rcpBrightness = 1.f / brightness;

			const float temporalVariance = MaxComponent(u_temporalVariance.GatherRed(u_nearestSampler, uv) * validDepthMask);
			const float temporalStdDev = sqrt(temporalVariance);

			const float currentFrameWeight = rcp(float(MAX_ACCUMULATED_FRAMES_NUM));

			const float noiseLevel = temporalStdDev * rcpBrightness * rtReflectionsInfluence * currentFrameWeight;

			const float depthDDX = max(linearDepth2x2.y - linearDepth2x2.x, linearDepth2x2.z - linearDepth2x2.x);
			const float depthDDY = max(linearDepth2x2.w - linearDepth2x2.x, linearDepth2x2.z - linearDepth2x2.x);

			uint maxVariableRate = SPT_VARIABLE_RATE_4X4;
			if(noiseLevel >= 0.08f)
			{
				maxVariableRate = SPT_VARIABLE_RATE_1X1;
			}
			else if (noiseLevel >= 0.04f)
			{
				maxVariableRate = depthDDX > depthDDY ? SPT_VARIABLE_RATE_2Y : SPT_VARIABLE_RATE_2X;
			}
			else if (noiseLevel >= 0.02f)
			{
				maxVariableRate = SPT_VARIABLE_RATE_2X2;
			}
			else if(noiseLevel >= 0.01f)
			{
				maxVariableRate = depthDDX > depthDDY ? (SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_4Y) : (SPT_VARIABLE_RATE_4X | SPT_VARIABLE_RATE_2Y);
			}

			variableRate = MinVariableRate(variableRate, maxVariableRate);
		}

		return variableRate;
	}
};


[numthreads(GROUP_SIZE_X * GROUP_SIZE_Y, 1, 1)]
void CreateVariableRateTextureCS(CS_INPUT input)
{
	VariableRateBuilder::BuildVariableRateTexture<RTVariableRateCallback>(input.groupID.xy, input.localID.x);
}
