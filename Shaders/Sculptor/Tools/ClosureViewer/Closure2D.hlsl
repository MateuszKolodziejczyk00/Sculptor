#include "SculptorShader.hlsli"

[[shader_params(ClosureViewerData, VIEWER)]]
[[shader_params(Closure2DConstants, CONSTS)]]

#include "Shading/Shading.hlsli"
#include "Tools/ClosureViewer/ClosureViewerCommon.hlsli"
#include "Utils/ColorSpaces.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void Closure2DCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	const uint2 outputRes = CONSTS->rwOutput.GetResolution();

	const float2 uv = (coords + 0.5f) / float2(outputRes);
	
	const float3 N = UP_VECTOR;
	const float3 V = VIEWER->viewDirection;

	const float phiL   = uv.x * 2.f * PI;
	const float thetaL = uv.y * 0.5f * PI;

	const float3 L = float3(sin(thetaL) * cos(phiL), sin(thetaL) * sin(phiL), cos(thetaL));

	const ShadedSurface surface = CreateViewerSurface(N);
	const BRDFLobes lobes = OpaqueClosure::Evaluate<false>(surface, V, L);

	float3 color = lobes.diffuse + lobes.specular;

	const float3 colorYCoCg = RGBToYCoCg(color);

	color = YCoCgToRGB(colorYCoCg / (colorYCoCg.x + 1.f));

	CONSTS->rwOutput[coords] = float4(color, 1.f);
}
