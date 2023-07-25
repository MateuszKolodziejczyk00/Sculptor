#include "Lights/LightsTiles.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Lights/Shadows.hlsli"
#include "Atmosphere/Atmosphere.hlsli"


// Based on https://themaister.net/blog/2020/01/10/clustered-shading-evolution-in-granite/
uint ClusterMaskRange(uint mask, uint2 range, uint startIdx)
{
	range.x = clamp(range.x, startIdx, startIdx + 32u);
	range.y = clamp(range.y + 1u, range.x, startIdx + 32u);

	const uint numBits = range.y - range.x;
	const uint rangeMask = numBits == 32 ? 0xffffffffu : ((1u << numBits) - 1u) << (range.x - startIdx);
	return mask & uint(rangeMask);
}

float3 CalcReflectedLuminance(ShadedSurface surface, float3 viewLocation)
{
    float3 luminance = 0.f;

    const float3 viewDir = normalize(viewLocation - surface.location);

    const float3 locationInAtmosphere = GetLocationInAtmosphere(u_atmosphereParams, surface.location);

    // Directional Lights

    for (uint i = 0; i < u_lightsData.directionalLightsNum; ++i)
    {
        const DirectionalLightGPUData directionalLight = u_directionalLights[i];

        const float3 illuminance = directionalLight.color * directionalLight.illuminance;

        if (any(illuminance > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
        {
            const float3 transmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, locationInAtmosphere, -directionalLight.direction);

            if (all(transmittance) == 0.f)
            {
                continue;
            }

            float visibility = 1.f;
            if(directionalLight.shadowMaskIdx != IDX_NONE_32)
            {
                visibility = u_shadowMasks[directionalLight.shadowMaskIdx].SampleLevel(u_nearestSampler, surface.uv, 0).x;
            }

            if(visibility > 0.f)
            {
                luminance += CalcLighting(surface, -directionalLight.direction, viewDir, illuminance) * visibility;
            }
        }
    }

    // Point lights
    const uint2 lightsTileCoords = GetLightsTile(surface.uv, u_lightsData.tileSize);
    const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);
    
    const uint clusterIdx = surface.linearDepth / u_lightsData.zClusterLength;
    const uint2 clusterRange = clusterIdx < u_lightsData.zClustersNum ? u_clustersRanges[clusterIdx] : uint2(0u, 0u);
    
    for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
    {
        uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];
        lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

        while(lightsMask)
        {
            const uint maskBitIdx = firstbitlow(lightsMask);

            const uint lightIdx = i * 32 + maskBitIdx;
            const PointLightGPUData pointLight = u_localLights[lightIdx];
          
            const float3 toLight = pointLight.location - surface.location;

            if (dot(toLight, surface.shadingNormal) > 0.f)
            {
                const float distToLight = length(toLight);

                if (distToLight < pointLight.radius)
                {
                    const float3 lightDir = toLight / distToLight;
                    const float3 illuminance = GetPointLightIlluminanceAtLocation(pointLight, surface.location);

                    if(any(illuminance > 0.f))
                    {
                        float visibility = 1.f;
                        if (pointLight.shadowMapFirstFaceIdx != IDX_NONE_32)
                        {
                            visibility = EvaluatePointLightShadows(surface, pointLight.location, pointLight.radius, pointLight.shadowMapFirstFaceIdx);
                        }
                
                        if (visibility > 0.f)
                        {
                            luminance += CalcLighting(surface, lightDir, viewDir, illuminance) * visibility;
                        }
                    }
                }
            }

            lightsMask &= ~(1u << maskBitIdx);
        }
    }

    // ambient
    luminance += surface.diffuseColor * u_lightsData.ambientLightIntensity;

    return luminance;
}


struct InScatteringParams
{
    float2 uv;
    float linearDepth;
    
    float3 worldLocation;

    float3 toViewNormal;

    float phaseFunctionAnisotrophy;

    float3 inScatteringColor;
};


float3 ComputeLocalLightsInScattering(in InScatteringParams params)
{
    float3 inScattering = 0.f;
    
    // Point lights
    const uint2 lightsTileCoords = GetLightsTile(params.uv, u_lightsData.tileSize);
    const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);
    
    const uint clusterIdx = params.linearDepth / u_lightsData.zClusterLength;
    const uint2 clusterRange = clusterIdx < u_lightsData.zClustersNum ? u_clustersRanges[clusterIdx] : uint2(0u, 0u);
    
    for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
    {
        uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];
        lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

        while(lightsMask)
        {
            const uint maskBitIdx = firstbitlow(lightsMask);

            const uint lightIdx = i * 32 + maskBitIdx;
            const PointLightGPUData pointLight = u_localLights[lightIdx];
          
            const float3 toLight = pointLight.location - params.worldLocation;

            const float distToLight = length(toLight);

            if(distToLight < pointLight.radius)
            {
                const float3 lightDir = toLight / distToLight;
                const float3 illuminance = GetPointLightIlluminanceAtLocation(pointLight, params.worldLocation);

                if (any(illuminance > 0.f))
                {
                    float visibility = 1.f;
                    if (pointLight.shadowMapFirstFaceIdx != IDX_NONE_32)
                    {
                        visibility = EvaluatePointLightShadowsAtLocation(params.worldLocation, pointLight.location, pointLight.radius, pointLight.shadowMapFirstFaceIdx);
                    }
            
                    if (visibility > 0.f)
                    {
                        inScattering += illuminance * visibility * PhaseFunction(params.toViewNormal, -lightDir, params.phaseFunctionAnisotrophy);
                    }
                }
            }

            lightsMask &= ~(1u << maskBitIdx);
        }
    }

    inScattering *= params.inScatteringColor;
    
    return inScattering;
}
