#include "SculptorShader.hlsli"

[[descriptor_set(TonemappingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/Exposure.hlsli"
#include "Utils/TonemappingOperators.hlsli"
#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


// Based on: https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html#/136/0/5
// Real-Time Samurai Cinema by J. Patry
float3 ApplyLocalExposure(in float3 linearColor, float2 uv)
{
	const float3 exposedColor = ExposedLuminanceToLuminance(linearColor);
	const float luminance = Luminance(exposedColor);

	const float logLuminance = log2(luminance + 0.0001f);

	const float bilateralGridDepth = saturate((logLuminance - u_tonemappingSettings.minLogLuminance) * u_tonemappingSettings.inverseLogLuminanceRange);

	const float downsampledLocalLogLuminance = u_logLuminanceTexture.SampleLevel(u_linearSampler, uv, 0).x;

	const float2 bilateralGridValue = u_luminanceBilateralGridTexture.SampleLevel(u_linearSampler, float3(uv, bilateralGridDepth), 0.f);
	const float gridLocalLogLuminance = bilateralGridValue.x / (bilateralGridValue.y + 0.0001f);

	const float localLogLuminance = lerp(downsampledLocalLogLuminance, gridLocalLogLuminance, u_tonemappingSettings.bilateralGridStrength);

	const float imageLogLuminance = GetAverageLogLuminance();

	const float contrastStrength = u_tonemappingSettings.contrastStrength;
	const float detailStrength   = u_tonemappingSettings.detailStrength;

	const float outputLogLuminance = contrastStrength * (localLogLuminance - imageLogLuminance) + detailStrength * (logLuminance - localLogLuminance) + imageLogLuminance;
	const float outputLuminance = exp2(outputLogLuminance);

	return linearColor * (outputLuminance / luminance);
}


[numthreads(8, 8, 1)]
void TonemappingCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_LDRTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 pixelSize = rcp(float2(outputRes));
  
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;
		float3 color = u_linearColorTexture.SampleLevel(u_sampler, uv, 0).xyz;
		color = ApplyLocalExposure(color, uv);

		color = float3(GTTonemapper(color.r), GTTonemapper(color.g), GTTonemapper(color.b));

		color = LinearTosRGB(color);

		if(u_tonemappingSettings.enableColorDithering)
		{
			color += Random(pixel) * rcp(255.f);
		}

		u_LDRTexture[pixel] = float4(color, 1.f);
	}
}