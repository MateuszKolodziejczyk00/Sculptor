#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]

[[shader_params(TonemappingPassConstants, u_tonemappingConstants)]]

#include "Utils/Exposure.hlsli"
#include "Utils/TonemappingOperators.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/ColorSpaces.hlsli"
#include "Utils/Random.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float3 TonyMCMapface(float3 stimulus)
{
    // Apply a non-linear transform that the LUT is encoded with.
    const float3 encoded = stimulus / (stimulus + 1.0);

    // Align the encoded range to texel centers.
    const float LUT_DIMS = 48.0;
    const float3 uv = encoded * ((LUT_DIMS - 1.0) / LUT_DIMS) + 0.5 / LUT_DIMS;

	return u_tonemappingConstants.tonemappingLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), uv);
}


// Based on: https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html#/136/0/5
// Real-Time Samurai Cinema by J. Patry
float3 ApplyLocalExposure(in float3 linearColor, in int2 pixel)
{
	const float3 exposedColor = ExposedLuminanceToLuminance(linearColor);
	const float luminance = Luminance(exposedColor);

	const float logLuminance = log2(luminance + 0.0001f);

	const float bilateralGridDepth = saturate((logLuminance - u_tonemappingConstants.minLogLuminance) * u_tonemappingConstants.inverseLogLuminanceRange);

	const float2 bilateralGridUV = pixel * u_tonemappingConstants.bilateralGridUVPerPixel;

	const float downsampledLocalLogLuminance = u_tonemappingConstants.logLuminance.SampleLevel(BindlessSamplers::LinearClampEdge(), bilateralGridUV, 0).x;

	const float2 bilateralGridValue = u_tonemappingConstants.luminanceBilateralGrid.SampleLevel(BindlessSamplers::LinearClampEdge(), float3(bilateralGridUV, bilateralGridDepth), 0.f);
	const float gridLocalLogLuminance = bilateralGridValue.x / (bilateralGridValue.y + 0.0001f);

	const float localLogLuminance = lerp(downsampledLocalLogLuminance, gridLocalLogLuminance, u_tonemappingConstants.bilateralGridStrength);

	const float imageLogLuminance = GetAverageLogLuminance();

	const float contrastStrength = u_tonemappingConstants.contrastStrength;
	const float detailStrength   = u_tonemappingConstants.detailStrength;

	const float outputLogLuminance = contrastStrength * (localLogLuminance - imageLogLuminance) + detailStrength * (logLuminance - localLogLuminance) + imageLogLuminance;
	const float outputLuminance = exp2(outputLogLuminance);

	return linearColor * (outputLuminance / luminance);
}


[numthreads(8, 8, 1)]
void TonemappingCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	float3 color = u_tonemappingConstants.linearColor.Load(coords).rgb;
	color = ApplyLocalExposure(color, coords);

	color = TonyMCMapface(color);

	if(u_tonemappingConstants.debugGeometry.IsValid())
	{
		const float2 uv = coords * u_tonemappingConstants.pixelSize;
		const float4 debugGeometry = u_tonemappingConstants.debugGeometry.SampleLevel(BindlessSamplers::LinearClampEdge(), uv);
		color = lerp(color, debugGeometry.rgb, debugGeometry.a);
	}

	color = LinearTosRGB(color);

	if(u_tonemappingConstants.enableColorDithering)
	{
		color += Random(coords) * rcp(255.f);
	}

	u_tonemappingConstants.rwLDRTexture.Store(coords, float4(color, 1.f));
}
