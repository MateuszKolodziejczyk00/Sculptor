#include "Utils/Shapes.hlsli"

[[shader_struct(AtmosphereParams)]]

// Based on https://www.shadertoy.com/view/slSXRW


float GetMiePhase(float cosTheta)
{
    const float g = 0.8f;
    const float scale = 3.0f / (8.0f * PI);

    const float g2 = Pow2(g);

    const float num = (1.0f - g2) * (1.0f + cosTheta * cosTheta);
    const float denom = (2.f + g2) * pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f);

    return scale * num / denom;
}


float GetRayleighPhase(float cosTheta)
{
    const float k = 3.f / (16.f * PI);
    return k * (1.f + cosTheta * cosTheta);
}


struct ScatteringValues
{
    float3 rayleighScattering;
    float mieScattering;
    float3 extinction;
};


ScatteringValues ComputeScatteringValues(in AtmosphereParams atmosphere, float3 locationMM)
{
    const float altitudeKM = (length(locationMM) - atmosphere.groundRadiusMM) * 1000.f;

    const float rayleighDensity = exp(-altitudeKM / 8.f);
    const float mieDensity = exp(-altitudeKM / 1.2f);

    ScatteringValues result;
    result.rayleighScattering = atmosphere.rayleighScattering * rayleighDensity;
    result.mieScattering = atmosphere.mieScattering * mieDensity;

    const float rayleighAbsorption = atmosphere.rayleighAbsorption * rayleighDensity;
    const float mieAbsorption = atmosphere.mieAbsorption * mieDensity;

    const float ozoneDensity = max(0.f, 1.f - abs(altitudeKM - 25.f) / 15.f);
    const float3 ozoneAbsorption = atmosphere.ozoneAbsorption * ozoneDensity;

    result.extinction = rayleighAbsorption + mieAbsorption + ozoneAbsorption + result.rayleighScattering + result.mieScattering;

    return result;
}


float2 ComputeAtmosphereLUTsUV(in AtmosphereParams atmosphere, in float3 location, in float3 sunDirection)
{
    const float height = length(location);
    const float3 upDirection = location / height;
    const float cosSunZenithAngle = dot(upDirection, sunDirection);
    
    const float u = cosSunZenithAngle * 0.5f + 0.5f;
    const float v = saturate((height - atmosphere.groundRadiusMM) / (atmosphere.atmosphereRadiusMM - atmosphere.groundRadiusMM));

    return float2(u, v);
}


float3 GetTransmittanceFromLUT(in AtmosphereParams atmosphere, in Texture2D<float3> transmittanceLUT, in SamplerState lutSampler, in float3 location, in float3 sunDirection)
{
    const float2 uv = ComputeAtmosphereLUTsUV(atmosphere, location, sunDirection);
    return transmittanceLUT.SampleLevel(lutSampler, uv, 0);
}


float3 GetMultiScatteringPsiFromLUT(in AtmosphereParams atmosphere, in Texture2D<float3> multiScatteringLUT, in SamplerState lutSampler, in float3 location, in float3 sunDirection)
{
    const float2 uv = ComputeAtmosphereLUTsUV(atmosphere, location, sunDirection);
    return multiScatteringLUT.SampleLevel(lutSampler, uv, 0);
}