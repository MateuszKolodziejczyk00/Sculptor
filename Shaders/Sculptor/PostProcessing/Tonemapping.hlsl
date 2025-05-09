#include "SculptorShader.hlsli"

[[descriptor_set(TonemappingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

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

    return u_tonemappingLUT.SampleLevel(u_linearSampler, uv, 0);
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

	const float downsampledLocalLogLuminance = u_logLuminanceTexture.SampleLevel(u_linearSampler, bilateralGridUV, 0).x;

	const float2 bilateralGridValue = u_luminanceBilateralGridTexture.SampleLevel(u_linearSampler, float3(bilateralGridUV, bilateralGridDepth), 0.f);
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
	const int2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_LDRTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 pixelSize = rcp(float2(outputRes));

		float3 color = u_linearColorTexture.Load(int3(pixel, 0)).rgb;
		color = ApplyLocalExposure(color, pixel);

		color = TonyMCMapface(color);

		color = LinearTosRGB(color);

		if(u_tonemappingConstants.enableColorDithering)
		{
			color += Random(pixel) * rcp(255.f);
		}

		u_LDRTexture[pixel] = float4(color, 1.f);
	}
}