#include "SculptorShader.hlsli"

[[shader_params(GenerateSpecularColorConstants, PARAMS_GENERATE_SPECULAR_COLOR)]]

#include "Shading/Shading.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void GenerateSpecularColorCS(CS_INPUT input)
{
	const uint2 coords = min(input.globalID.xy, PARAMS_GENERATE_SPECULAR_COLOR->resolution - 1u);

	const float4 baseColorMetallic = PARAMS_GENERATE_SPECULAR_COLOR->baseColorMetallic.Load(uint3(coords, 0u));

	float3 specularColor = 0.f;
	float3 diffuseColor = 0.f;

	ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.a, OUT diffuseColor, OUT specularColor);

	PARAMS_GENERATE_SPECULAR_COLOR->rwSpecularColor[coords] = specularColor;
}
