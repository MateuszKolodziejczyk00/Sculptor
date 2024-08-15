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
	static uint ComputeVariableRateMask(in VariableRateProcessor processor, in float2 edgeFilter)
	{
		const float rtReflectionsInfluence = u_influenceTexture.Load(int3(processor.GetCoords(), 0)).x;

		const float roughness = u_roughnessTexture.Load(int3(processor.GetCoords(), 0)).x;
		const float roughnessInfluence = lerp(1.f, 2.f, 1.f - roughness);

		const float finalInfluence = rtReflectionsInfluence * roughnessInfluence;

		uint variableRate = processor.ComputeEdgeBasedVariableRateMask(edgeFilter * finalInfluence);

		if(roughness < 0.015f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_1X1);
		}
		else if (roughness <= 0.05f)
		{
			variableRate = MinVariableRate(variableRate, SPT_VARIABLE_RATE_2X2);
		}

		return variableRate;
	}
};


[numthreads(GROUP_SIZE_X * GROUP_SIZE_Y, 1, 1)]
void CreateVariableRateTextureCS(CS_INPUT input)
{
	VariableRateBuilder::BuildVariableRateTexture<RTVariableRateCallback>(input.groupID.xy, input.localID.x);
}
