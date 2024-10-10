#include "SculptorShader.hlsli"

[[descriptor_set(RTVariableRateTextureDS, 1)]]

#include "Utils/VariableRate/VariableRateBuilder.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};



float GetNoiseMulitplierForRoughness(float roughness)
{
	return lerp(0.3f, 1.f, smoothstep(0.3f, 0.6f, roughness));
}


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

			float spatioTemporalVariance = u_spatioTemporalVariance.Load(int3(processor.GetCoords(), 0)).x;
			spatioTemporalVariance = processor.QuadMax(spatioTemporalVariance);
			const float spatioTemporalStdDev = sqrt(spatioTemporalVariance);

			float noiseLevel = spatioTemporalStdDev * rcpBrightness * rtReflectionsInfluence;

			noiseLevel *= GetNoiseMulitplierForRoughness(roughness);

			const float linearDepth = u_linearDepthTexture.Load(int3(processor.GetCoords(), 0)).x;
			const float depthDDX = processor.DDX_QuadMax(linearDepth);
			const float depthDDY = processor.DDY_QuadMax(linearDepth);

			uint minVariableRate = SPT_VARIABLE_RATE_4X4;
			if(noiseLevel >= 0.6f)
			{
				minVariableRate = SPT_VARIABLE_RATE_1X1;
			}
			else if (noiseLevel >= 0.45f)
			{
				minVariableRate = depthDDX > depthDDY ? SPT_VARIABLE_RATE_2Y : SPT_VARIABLE_RATE_2X;
			}
			else if (noiseLevel >= 0.15f)
			{
				minVariableRate = SPT_VARIABLE_RATE_2X2;
			}
			else if(noiseLevel >= 0.08f)
			{
				minVariableRate = depthDDX > depthDDY ? (SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_4Y) : (SPT_VARIABLE_RATE_4X | SPT_VARIABLE_RATE_2Y);
			}

			variableRate = MinVariableRate(variableRate, minVariableRate);
		}

		if(WaveActiveAnyTrue(variableRate > SPT_VARIABLE_RATE_2X2))
		{
			const uint2 resamplingTileCoords        = processor.GetCoords() / uint2(8u, 4u);
			const uint2 resamplingTileTextureCoords = resamplingTileCoords / uint2(8u, 4u);
			const uint  resamplingTileBitOffset     = dot(resamplingTileCoords & uint2(7u, 3u), uint2(1u, 8u));
			const uint tileReliabilityMask          = u_tileReliabilityMask.Load(int3(resamplingTileTextureCoords, 0)).x;
			const bool isTileUnreliable             = (tileReliabilityMask & (1u << resamplingTileBitOffset)) == 0u;

			if(isTileUnreliable)
			{
				variableRate = (variableRate >> 2u);
			}
		}

		const float volatileQuadMinRoughness = 0.5f;
		if(WaveActiveAnyTrue(variableRate > SPT_VARIABLE_RATE_2X2) && roughness > volatileQuadMinRoughness)
		{
			const uint2 quadCoords = processor.GetCoords() / 2u;
			const uint2 quadTextureCoords = quadCoords / uint2(8u, 4u);
			const uint quadBitOffset = dot(quadCoords & uint2(7u, 3u), uint2(1u, 8u));
			
			const uint quadVolatilityMask = u_quadVolatilityMask.Load(int3(quadTextureCoords, 0)).x;
			const bool isQuadVolatile = (quadVolatilityMask & (1u << quadBitOffset)) != 0u;

			if(isQuadVolatile && roughness > volatileQuadMinRoughness)
			{
				variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_2X2);
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
