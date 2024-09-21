#include "SculptorShader.hlsli"

[[descriptor_set(RTVariableRateTextureDS, 1)]]

#include "Utils/VariableRate/VariableRateBuilder.hlsli"


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

		float rtReflectionsInfluence = u_influenceTexture.Load(int3(processor.GetCoords(), 0)).x;
		rtReflectionsInfluence = processor.QuadMax(rtReflectionsInfluence);

		float roughness = u_roughnessTexture.Load(int3(processor.GetCoords(), 0)).x;
		roughness = processor.QuadMin(roughness);

		uint variableRate = SPT_VARIABLE_RATE_4X4;

		if(roughness < 0.015f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_1X1);
		}
		else if (roughness <= 0.05f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_2X2);
		}

		if(WaveActiveAnyTrue(variableRate != SPT_VARIABLE_RATE_1X1))
		{
			float brightness = Luminance(u_reflectionsTexture.Load(int3(processor.GetCoords(), 0)));
			brightness = processor.QuadMax(brightness);
			const float rcpBrightness = 1.f / brightness;

			const float2 temporalMoments = u_temporalMomentsTexture.Load(int3(processor.GetCoords(), 0)).xy;
			float temporalVariance = abs(temporalMoments.y - Pow2(temporalMoments.x));
			temporalVariance = processor.QuadMax(temporalVariance);
			const float temporalStdDev = sqrt(temporalVariance);

			float spatialStdDev = u_spatialStdDevTexture.Load(int3(processor.GetCoords(), 0)).x;
			spatialStdDev = processor.QuadMax(spatialStdDev);

			float stdDev = spatialStdDev * rcpBrightness;
			stdDev *= min(temporalStdDev, 2.f) * rtReflectionsInfluence;

			const float linearDepth = u_linearDepthTexture.Load(int3(processor.GetCoords(), 0)).x;
			const float depthDDX = processor.DDX_QuadMax(linearDepth);
			const float depthDDY = processor.DDY_QuadMax(linearDepth);

			uint minVariableRate = SPT_VARIABLE_RATE_4X4;
			if(stdDev >= 0.25f)
			{
				minVariableRate = SPT_VARIABLE_RATE_1X1;
			}
			else if (stdDev >= 0.16f)
			{
				minVariableRate = depthDDX > depthDDY ? SPT_VARIABLE_RATE_2Y : SPT_VARIABLE_RATE_2X;
			}
			else if (stdDev >= 0.08f)
			{
				minVariableRate = SPT_VARIABLE_RATE_2X2;
			}
			else if(stdDev >= 0.035f)
			{
				minVariableRate = depthDDX > depthDDY ? (SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_4Y) : (SPT_VARIABLE_RATE_4X | SPT_VARIABLE_RATE_2Y);
			}

			variableRate = MinVariableRate(variableRate, minVariableRate);

			if (WaveActiveAnyTrue(variableRate != SPT_VARIABLE_RATE_1X1))
			{
				float geometryCoherence = u_geometryCoherenceTexture.Load(int3(processor.GetCoords(), 0));
				geometryCoherence = processor.QuadMin(geometryCoherence);

				const float stdDevThreshold = lerp(0.0001f, 0.01f, roughness);

				if(geometryCoherence < 3.f && stdDev >= stdDevThreshold)
				{
					variableRate = SPT_VARIABLE_RATE_1X1;
				}
			}
		}

		return variableRate;
	}
};


[numthreads(GROUP_SIZE_X * GROUP_SIZE_Y, 1, 1)]
void CreateVariableRateTextureCS(CS_INPUT input)
{
	VariableRateBuilder::BuildVariableRateTexture<RTVariableRateCallback>(input.groupID.xy, input.localID.x);
}
