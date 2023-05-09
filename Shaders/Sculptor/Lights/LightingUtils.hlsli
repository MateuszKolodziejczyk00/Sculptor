#ifndef LIGHTING_UTILS_H
#define LIGHTING_UTILS_H

#include "Shading/Shading.hlsli"

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


float3 GetPointLightIntensityAtLocation(PointLightGPUData pointLight, float3 location)
{
    return GetPointLightAttenuationAtLocation(pointLight, location) * pointLight.intensity * pointLight.color;
}


float3 CalcLighting(ShadedSurface surface, float3 lightDir, float3 viewDir, float3 peakIrradiance)
{
    return DoShading(surface, lightDir, viewDir, peakIrradiance);
}


#endif // LIGHTING_UTILS_H
