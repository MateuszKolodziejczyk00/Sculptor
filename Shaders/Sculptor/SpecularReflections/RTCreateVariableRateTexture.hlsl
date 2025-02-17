#include "SculptorShader.hlsli"

[[descriptor_set(RTVariableRateTextureDS, 1)]]

#define VR_BUILDER_SINGLE_LANE_PER_QUAD 0

#include "Utils/VariableRate/VariableRateBuilder.hlsli"
#include "Utils/Wave.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID  : SV_GroupID;
	uint3 localID  : SV_GroupThreadID;
};


// Based on slide 36 from "Software Based Variable Rate Shading in Call of Duty: Modern Warfare" by Michal Drobot
float2 EdgeFilter(in VariableRateProcessor processor, in float brightness)
{
	const uint laneIdx = WaveGetLaneIndex();

	const uint3 coords = uint3(processor.GetCoords(), 0u);

	// In theory this can be problematic as we ignore diffuse here but 
	// both diffuse and specular use same ray in case of mirrors so computing edge from one of them should be representative
	float upperLeft  = Luminance(u_specularReflectionsTexture.Load(coords, int2(-1,  1)));
	float upperRight = Luminance(u_specularReflectionsTexture.Load(coords, int2( 1,  1)));
	float lowerLeft  = Luminance(u_specularReflectionsTexture.Load(coords, int2(-1, -1)));
	float lowerRight = Luminance(u_specularReflectionsTexture.Load(coords, int2( 1, -1)));

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

		const float2 rtReflectionsInfluence = u_influenceTexture.Load(coords);
 
		const float roughness = u_roughnessTexture.Load(coords);

		const float specularBrightness = Luminance(u_specularReflectionsTexture.Load(coords));
		const float diffuseBrightness  = Luminance(u_diffuseReflectionsTexture.Load(coords));

		uint variableRate = SPT_VARIABLE_RATE_4X4;

		if(roughness < 0.015f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_1X1);
			const float threshold = 0.08f;
			const float2 edgeFilter = EdgeFilter(processor, specularBrightness) * (rtReflectionsInfluence.x + rtReflectionsInfluence.y);
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
			const float2 varianceEstimation = u_varianceEstimation.Load(coords);
			const float specularStdDevEstimation = sqrt(varianceEstimation.x);
			const float diffuseStdDevEstimation  = sqrt(varianceEstimation.y);

			const bool enableSpecularBoost = true;
			const float roughnessBoostThreshold = 0.35f;
			const float boostStrength = 2.f;
			const float boostAmount = (max(0.f, roughnessBoostThreshold - roughness) / roughnessBoostThreshold) * boostStrength;
			const float specularNoiseBoost = enableSpecularBoost ? boostAmount + 1.f : 1.f;

			const float specularNoiseAbsoluteLevel = specularStdDevEstimation * rtReflectionsInfluence.x * specularNoiseBoost;
			const float diffuseNoiseAbsoluteLevel  = diffuseStdDevEstimation * rtReflectionsInfluence.y;

			const float specularNoiseRelativeLevel = specularNoiseAbsoluteLevel / specularBrightness;
			const float diffuseNoiseRelativeLevel  = diffuseNoiseAbsoluteLevel / diffuseBrightness;

			const float absoluteNoiseWeight = 0.3f;
			const float relativeNoiseWeight = 1.f - absoluteNoiseWeight;

			const float specularNoiseLevel = specularNoiseAbsoluteLevel * absoluteNoiseWeight + specularNoiseRelativeLevel * relativeNoiseWeight;
			const float diffuseNoiseLevel  = diffuseNoiseAbsoluteLevel * absoluteNoiseWeight + diffuseNoiseRelativeLevel * relativeNoiseWeight;

			const float noiseLevel = diffuseNoiseLevel + specularNoiseLevel;

			const float depthDDX = processor.DDX_QuadMax(linearDepth);
			const float depthDDY = processor.DDY_QuadMax(linearDepth);

			uint maxVariableRate = SPT_VARIABLE_RATE_4X4;
			if(noiseLevel >= 1.5f)
			{
				maxVariableRate = SPT_VARIABLE_RATE_1X1;
			}
			else if (noiseLevel >= 0.8f)
			{
				maxVariableRate = depthDDX > depthDDY ? SPT_VARIABLE_RATE_2Y : SPT_VARIABLE_RATE_2X;
			}
			else if (noiseLevel >= 0.35f)
			{
				maxVariableRate = SPT_VARIABLE_RATE_2X2;
			}
			else if(noiseLevel >= 0.15f)
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
