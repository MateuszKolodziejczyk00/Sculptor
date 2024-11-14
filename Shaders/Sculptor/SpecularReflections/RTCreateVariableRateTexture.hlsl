#include "SculptorShader.hlsli"

[[descriptor_set(RTVariableRateTextureDS, 1)]]

#define VR_BUILDER_SINGLE_LANE_PER_QUAD 0

#include "Utils/VariableRate/VariableRateBuilder.hlsli"
#include "Utils/Wave.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};


float ComputeRoughnessNoiseLevelMultiplier(in float roughness)
{
	return lerp(1.f, 0.5f, smoothstep(0.2f, 0.4f, roughness));
}


// Based on slide 36 from "Software Based Variable Rate Shading in Call of Duty: Modern Warfare" by Michal Drobot
float2 EdgeFilter(in VariableRateProcessor processor, in float brightness)
{
	const uint laneIdx = WaveGetLaneIndex();

	const uint3 coords = uint3(processor.GetCoords(), 0u);

	float upperLeft  = Luminance(u_reflectionsTexture.Load(coords, int2(-1,  1)));
	float upperRight = Luminance(u_reflectionsTexture.Load(coords, int2( 1,  1)));
	float lowerLeft  = Luminance(u_reflectionsTexture.Load(coords, int2(-1, -1)));
	float lowerRight = Luminance(u_reflectionsTexture.Load(coords, int2( 1, -1)));

	if(laneIdx & 0x1)
	{
		Swap(lowerLeft, lowerRight);
		Swap(upperLeft, upperRight);
	}
	if(laneIdx & 0x2)
	{
		Swap(lowerLeft, upperLeft);
		Swap(lowerRight, upperRight);
	}

	const uint4 horizontalSwizzle = uint4(1u, 0u, 3u, 2u);
	const uint4 verticalSwizzle   = uint4(2u, 3u, 0u, 1u);
	const uint4 diagonalSwizzle   = uint4(3u, 2u, 1u, 0u);

	const uint swizzleQuadBase = laneIdx & ~0x3;
	const uint quadLocalID     = laneIdx & 0x3;

	const float up   = QuadSwizzle(upperRight, swizzleQuadBase, quadLocalID, horizontalSwizzle);
	const float down = QuadSwizzle(lowerRight, swizzleQuadBase, quadLocalID, horizontalSwizzle);

	const float left  = QuadSwizzle(upperLeft, swizzleQuadBase, quadLocalID, verticalSwizzle);
	const float right = QuadSwizzle(upperRight, swizzleQuadBase, quadLocalID, verticalSwizzle);

	const float center = brightness;

	const float dx  = max(abs(left - center), abs(right - center));
	const float ddx = max(abs(upperRight - upperLeft), abs(lowerRight - lowerLeft));

	const float dy  = max(abs(up - center), abs(down - center));
	const float ddy = max(abs(upperRight - lowerRight), abs(upperLeft - lowerLeft));

	return float2(processor.QuadMax(max(dx, ddy)), processor.QuadMax(max(dy, ddy)));
}


struct RTVariableRateCallback
{
	static uint ComputeVariableRateMask(in VariableRateProcessor processor)
	{
		if(u_rtConstants.forceFullRateTracing)
		{
			return SPT_VARIABLE_RATE_1X1;
		}

		const uint3 coords = uint3(processor.GetCoords(), 0u);

		const float linearDepth = u_linearDepthTexture.Load(coords);
		const bool isDepthValid = linearDepth > 0.f;

		const float validDepthMask = select(isDepthValid, 1.f, 0.f);

		if(processor.QuadMax(validDepthMask) == 0.f)
		{
			return SPT_VARIABLE_RATE_4X4;
		}

		const float rtReflectionsInfluence = u_influenceTexture.Load(coords);
 
		const float roughness = u_roughnessTexture.Load(coords);

		const float brightness = Luminance(u_reflectionsTexture.Load(coords));

		const float minBrightness = processor.QuadMin(isDepthValid ? brightness : FLOAT_MAX);

		uint variableRate = SPT_VARIABLE_RATE_4X4;

		if(roughness < 0.015f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_1X1);
			const float threshold = minBrightness * 0.04f;
			const float2 edgeFilter = EdgeFilter(processor, brightness) * rtReflectionsInfluence;
			if(edgeFilter.x < threshold)
			{
				variableRate |= SPT_VARIABLE_RATE_2X;
			}
			if(edgeFilter.y < threshold)
			{
				variableRate |= SPT_VARIABLE_RATE_2Y;
			}
		}
		else if(variableRate != SPT_VARIABLE_RATE_1X1)
		{
			const float rcpBrightness = 1.f / minBrightness;

			const float varianceEstimation = u_varianceEstimation.Load(coords);
			const float stdDevEstimation   = sqrt(varianceEstimation);

			const float currentFrameWeight = rcp(float(MAX_ACCUMULATED_FRAMES_NUM));

			const float roughnessNoiseLevelMultiplier = ComputeRoughnessNoiseLevelMultiplier(roughness);

			const float noiseLevel = stdDevEstimation * rcpBrightness * rtReflectionsInfluence * currentFrameWeight * roughnessNoiseLevelMultiplier;
			const float quadNoiseLevel = processor.QuadMax(isDepthValid ? noiseLevel : FLOAT_MAX);

			const float depthDDX = processor.DDX_QuadMax(linearDepth);
			const float depthDDY = processor.DDY_QuadMax(linearDepth);

			uint maxVariableRate = SPT_VARIABLE_RATE_4X4;
			if(noiseLevel >= 0.07f)
			{
				maxVariableRate = SPT_VARIABLE_RATE_1X1;
			}
			else if (noiseLevel >= 0.035f)
			{
				maxVariableRate = depthDDX > depthDDY ? SPT_VARIABLE_RATE_2Y : SPT_VARIABLE_RATE_2X;
			}
			else if (noiseLevel >= 0.018f)
			{
				maxVariableRate = SPT_VARIABLE_RATE_2X2;
			}
			else if(noiseLevel >= 0.009f)
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
