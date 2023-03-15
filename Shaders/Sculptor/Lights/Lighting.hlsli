#include "Shading/Shading.hlsli"
#include "Lights/LightsTiles.hlsli"
#include "Lights/Shadows.hlsli"


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


float3 CalcReflectedRadiance(ShadedSurface surface, float3 viewLocation)
{
    float3 radiance = 0.f;

    const float3 viewDir = normalize(viewLocation - surface.location);

    // Directional Lights

    for (uint i = 0; i < u_lightsData.directionalLightsNum; ++i)
    {
        const DirectionalLightGPUData directionalLight = u_directionalLights[i];

        const float3 lightIntensity = directionalLight.color * directionalLight.intensity;

        if (any(lightIntensity > 0.f) || dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
        {
            float visibility = 0.f;
            if(directionalLight.shadowMaskIdx != IDX_NONE_32)
            {
                visibility = u_shadowMaps[directionalLight.shadowMaskIdx].SampleLevel(u_shadowMaskSampler, surface.uv, 0).x;
            }

            if(visibility > 0.f)
            {
                radiance += CalcLighting(surface, -directionalLight.direction, viewDir, lightIntensity) * visibility;
            }
        }
    }

    // Point lights
    const uint2 lightsTileCoords = GetLightsTile(surface.uv, u_lightsData.tileSize);
    const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);
    
    for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
    {
        uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];

        while(lightsMask)
        {
            const uint maskBitIdx = firstbitlow(lightsMask);

            const uint lightIdx = i * 32 + maskBitIdx;
            const PointLightGPUData pointLight = u_localLights[lightIdx];
          
            const float3 toLight = pointLight.location - surface.location;

            if(dot(toLight, surface.shadingNormal) > 0.f && dot(toLight, surface.geometryNormal) > 0.f)
            {
                const float distToLight = length(toLight);

                if(distToLight < pointLight.radius)
                {
                    const float3 lightDir = toLight / distToLight;
                    const float3 lightIntensity = GetPointLightIntensityAtLocation(pointLight, surface.location);

                    float visibility = 1.f;
                    if (pointLight.shadowMapFirstFaceIdx != IDX_NONE_32)
                    {
                        visibility = EvaluatePointLightShadows(surface, pointLight.location, pointLight.radius, pointLight.shadowMapFirstFaceIdx);
                    }
                
                    if (visibility > 0.f)
                    {
                        radiance += CalcLighting(surface, lightDir, viewDir, lightIntensity) * visibility;
                    }
                }
            }

            lightsMask &= ~(1u << maskBitIdx);
        }
    }

    // ambient
    radiance += surface.diffuseColor * u_lightsData.ambientLightIntensity;

    return radiance;
}
