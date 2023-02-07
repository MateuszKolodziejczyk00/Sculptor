#include "Shading/Shading.hlsli"
#include "Lights/LightsTiles.hlsli"


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


float3 CalcReflectedRadiance(ShadedSurface surface, float3 viewLocation, float2 screenUV)
{
    const uint2 lightsTileCoords = GetLightsTile(screenUV, u_lightsData.tileSize);
    const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);

    float3 radiance = 0.f;

    const float3 viewDir = normalize(surface.location - viewLocation);

    for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
    {
        uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];

        while(lightsMask)
        {
            const uint maskBitIdx = firstbitlow(lightsMask);

            const uint lightIdx = i * 32 + maskBitIdx;
            const PointLightGPUData pointLight = u_localLights[lightIdx];
          
            const float3 toLight = pointLight.location - surface.location;

            if(dot(toLight, surface.normal) > 0.f)
            {
                const float3 lightDir = normalize(toLight);
                const float3 lightIntensity = GetPointLightIntensityAtLocation(pointLight, surface.location);
                radiance += CalcLighting(surface, lightDir, viewDir, lightIntensity);
            }

            lightsMask &= ~(1u << maskBitIdx);
        }
    }

    // ambient
    radiance += surface.diffuseColor * 0.1f;

    return radiance;
}
