#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]

[[shader_params(TonemappingPassConstants, u_tonemappingConstants)]]

#include "Utils/Exposure.hlsli"
#include "Utils/TonemappingOperators.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/ColorSpaces.hlsli"
#include "Utils/Random.hlsli"
#include "GTTonemapper/GT7ToneMapper.hlsli"


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


float3 ApplyPurkinjeShift(float3 linearColor, float intensity)
{
	// based on "Real-Time Samurai Cinema: Lighting, Atmosphere, and Tonemapping in Ghost of Tsushima"
	// and "Perceptually Based Tone Mapping for Low-Light Conditions"

	const float scale = 1000.f;

	const float4 lmsr = RGBToLMSR(linearColor * scale);

	const float3 m = float3(0.63721f, 0.39242f, 1.6064f); // maximal cone sensitivity
	const float3 k = float3(0.2f, 0.2f, 0.29f); // rod input strength

	const float3 g = rsqrt(1.f + (0.3333f / m) * (lmsr.xyz + k * lmsr.w));

	const float3x3 A = float3x3(
		-1.f, 1.f, 0.f,
		-1.f, -1.f, 1.f,
		1.f, 1.f, 0.f
	);

	const float K = 45.f; // Scaling constant
	const float S = 10.f; // Static saturation
	const float k3 = 0.6f; // Surround strength or opponent signal
	const float rw = 0.139f; // Ratio of responses for white light
	const float p = 0.6189f; // Relative weight of L cones

	const float3x3 i = float3x3(
		-(k3 + rw), (1.f + rw * k3), 0.f,
		p * k3, (1.f - p) * k3, 1.f,
		p * S, (1.f - p) * S, 0.f
	);

	const float3 d_o = (K / S) * mul(mul(i, mul(Diag(k), Diag(rcp(m)))), g * lmsr.w);

	const float3x3 InverseA = float3x3(
		-0.5f,  0.0f,  0.5f,
		0.5f,  0.0f,  0.5f,
		0.0f,  1.0f,  1.0f
	);

	const float3 lmsGain = mul(InverseA, d_o);

	const float3 rgbGain = LMSR3ToRGB(lmsGain);

	return linearColor + rgbGain * intensity / scale;
}


// Based on: https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html#/136/0/5
// Real-Time Samurai Cinema by J. Patry
float3 ApplyLocalExposure(in float3 linearColor, in int2 pixel)
{
	float3 exposedColor = ExposedLuminanceToLuminance(linearColor);

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


float3 ApplyWhiteBalance(in float3 linearColorRGB, in float whitePointTemperature)
{
	// This applies von Kries chromatical adaptation transform
	// See: "The von Kries chromatic adaptation transform and its generalization"
	// We use Bradford's transformation matrices which improve adaptation of blue colors as mentioned in:
	// https://en.wikipedia.org/wiki/LMS_color_space

	const float3 whitePointXYZ = ComputePlanckLocus(whitePointTemperature);
	const float3 whitePointLMS = XYZToLMSBradford(whitePointXYZ);

	const float3 linearColorLMS = XYZToLMSBradford(RGBToXYZ(linearColorRGB));

	const float3 balancedColorLMS = linearColorLMS / whitePointLMS;

	return XYZToRGB(LMSToXYZBradford(balancedColorLMS));
}


[numthreads(8, 8, 1)]
void TonemappingCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	float3 color = u_tonemappingConstants.linearColor.Load(coords).rgb;

	if (u_tonemappingConstants.purkinjeShiftIntensity > 0.f)
	{
		color = ApplyPurkinjeShift(color, u_tonemappingConstants.purkinjeShiftIntensity);
	}

	color = ApplyLocalExposure(color, coords);

	color = ApplyWhiteBalance(color, u_tonemappingConstants.whitePointTemperature);

#if TONEMAPPER == 1
	GT7ToneMapping tonemapOp;
	tonemapOp.initializeAsSDR();
	color = Rec709ToRec2020(color);
	color = tonemapOp.applyToneMapping(color);
	color = Rec2020ToRec709(color);
#elif TONEMAPPER == 2
	color = TonyMCMapface(color);
#endif

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
