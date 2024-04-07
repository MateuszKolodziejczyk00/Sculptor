#ifndef LIGHTING_UTILS_H
#define LIGHTING_UTILS_H

#include "Shading/Shading.hlsli"


class SceneLightingAccumulator
{
	static SceneLightingAccumulator Create()
	{
		SceneLightingAccumulator accumulator;
		accumulator.sceneLuminance = 0.f;
		return accumulator;
	}

	void Accumulate(in LightingContribution contribution)
	{
		sceneLuminance += contribution.sceneLuminance;
	}

	void Accumulate(in float3 luminance)
	{
		sceneLuminance += luminance;
	}

	float3 GetLuminance()
	{
		return sceneLuminance;
	}

	float3 sceneLuminance;
};


class ViewLightingAccumulator
{
	static ViewLightingAccumulator Create()
	{
		ViewLightingAccumulator accumulator;
		accumulator.sceneLuminance         = 0.f;
		accumulator.eyeAdaptationLuminance = 0.f;
		return accumulator;
	}

	void Accumulate(in LightingContribution contribution)
	{
		sceneLuminance         += contribution.sceneLuminance;
		eyeAdaptationLuminance += contribution.eyeAdaptationLuminance;
	}

	float3 GetLuminance()
	{
		return sceneLuminance;
	}

	float3 GetEyeAdaptationLuminance()
	{
		return eyeAdaptationLuminance;
	}

	float3 sceneLuminance;
	float3 eyeAdaptationLuminance;
};


[[shader_struct(PointLightGPUData)]]


float HenyeyGreensteinPhaseFunction(in float g, in float cosTheta)
{
    const float numerator = 1.f - g * g;
    const float denominator = 4.f * PI * pow(1.f + g * g - 2.f * g * cosTheta, 1.5f);
    return numerator / denominator;
}


float PhaseFunction(in float3 toView, in float3 fromLight, float g)
{
    const float cosTheta = dot(toView, fromLight);
    return HenyeyGreensteinPhaseFunction(g, cosTheta);
}


float GetPointLightAttenuationAtLocation(PointLightGPUData pointLight, float3 location)
{
    const float distAlpha = saturate(1.f - length(pointLight.location - location) / pointLight.radius);
    return distAlpha * distAlpha;
}


float3 GetPointLightIlluminanceAtLocation(PointLightGPUData pointLight, float3 location)
{
    return GetPointLightAttenuationAtLocation(pointLight, location) * pointLight.color * pointLight.luminousPower / (4.f * PI);
}


LightingContribution CalcLighting(ShadedSurface surface, float3 lightDir, float3 viewDir, float3 peakIlluminance)
{
    return DoShading(surface, lightDir, viewDir, peakIlluminance);
}


#endif // LIGHTING_UTILS_H
