#include "Lights/LightsTiles.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Lights/Shadows.hlsli"
#include "Atmosphere/Atmosphere.hlsli"

#ifdef DS_DDGIDS
#include "DDGI/DDGITypes.hlsli"
#endif // DS_DDGIDS

#ifdef DS_ViewShadingInputDS

// Based on https://themaister.net/blog/2020/01/10/clustered-shading-evolution-in-granite/
uint ClusterMaskRange(uint mask, uint2 range, uint startIdx)
{
	range.x = clamp(range.x, startIdx, startIdx + 32u);
	range.y = clamp(range.y + 1u, range.x, startIdx + 32u);

	const uint numBits = range.y - range.x;
	const uint rangeMask = numBits == 32 ? 0xffffffffu : ((1u << numBits) - 1u) << (range.x - startIdx);
	return mask & uint(rangeMask);
}

float3 CalcReflectedLuminance(in ShadedSurface surface, in float3 viewDir)
{
    float3 luminance = 0.f;

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
                luminance += CalcLighting(surface, -directionalLight.direction, viewDir, illuminance) * visibility * transmittance;
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

    // Directional Lights

    const float3 locationInAtmosphere = GetLocationInAtmosphere(u_atmosphereParams, params.worldLocation);

    for (uint i = 0; i < u_lightsData.directionalLightsNum; ++i)
    {
        const DirectionalLightGPUData directionalLight = u_directionalLights[i];

        const float3 illuminance = directionalLight.color * directionalLight.illuminance;

        float visibility = 1.f;
        if(directionalLight.shadowCascadesNum != 0)
        {
            visibility = EvaluateCascadedShadowsAtLocation(params.worldLocation, directionalLight.firstShadowCascadeIdx, directionalLight.shadowCascadesNum);
        }

        if(visibility > 0.f)
        {
            const float3 transmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, locationInAtmosphere, -directionalLight.direction);
            inScattering += transmittance * illuminance * PhaseFunction(params.toViewNormal, directionalLight.direction, params.phaseFunctionAnisotrophy);
        }
    }
    
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

#endif // DS_ViewShadingInputDS

#ifdef DS_GlobalLightsDS

struct ShadowRayPayload
{
    bool isShadowed;
};


float3 CalcReflectedLuminance(ShadedSurface surface, float3 viewDir)
{
    float3 luminance = 0.f;

    // Directional Lights

    for (uint i = 0; i < u_lightsParams.directionalLightsNum; ++i)
    {
        const DirectionalLightGPUData directionalLight = u_directionalLights[i];

        const float3 lightIlluminance = directionalLight.color * directionalLight.illuminance;

        if (any(lightIlluminance > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
        {
            ShadowRayPayload payload = { true };

            RayDesc rayDesc;
            rayDesc.TMin        = 0.02f;
            rayDesc.TMax        = 50.f;
            rayDesc.Origin      = surface.location;
            rayDesc.Direction   = -directionalLight.direction;

            TraceRay(u_sceneTLAS,
                     RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                     0xFF,
                     0,
                     1,
                     1,
                     rayDesc,
                     payload);
            
            if (!payload.isShadowed)
            {
                luminance += CalcLighting(surface, -directionalLight.direction, viewDir, lightIlluminance);
            }
        }
    }

    // Point Lights

    for(uint lightIdx = 0; lightIdx < u_lightsParams.pointLightsNum; ++lightIdx)
    {
        const PointLightGPUData pointLight = u_pointLights[lightIdx];
        
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
                        const float3 biasedLocation  = surface.location + surface.shadingNormal * 0.02f;
                        visibility = EvaluatePointLightShadowsAtLocation(surface.location, biasedLocation, pointLight.radius, pointLight.shadowMapFirstFaceIdx);
                    }
            
                    if (visibility > 0.f)
                    {
                        luminance += CalcLighting(surface, lightDir, viewDir, illuminance) * visibility;
                    }
                }
            }
        }
    }

    // Indirect diffuse
#ifdef DS_DDGIDS
    DDGISampleParams diffuseSampleParams = CreateDDGISampleParams(surface.location, surface.geometryNormal, viewDir);
    diffuseSampleParams.sampleDirection = surface.shadingNormal;
    const float3 indirectIlluminance = DDGISampleIlluminance(u_ddgiParams, u_probesIlluminanceTexture, u_probesDataSampler, u_probesHitDistanceTexture, u_probesDataSampler, diffuseSampleParams);
    luminance += Diffuse_Lambert(indirectIlluminance) * surface.diffuseColor;

    const float NdotV = saturate(dot(surface.shadingNormal, viewDir));
    const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_brdfIntegrationLUTSampler, float2(NdotV, surface.roughness), 0);

    DDGISampleParams specularSampleParams = CreateDDGISampleParams(surface.location, surface.geometryNormal, viewDir);
    specularSampleParams.sampleDirection = reflect(-viewDir, surface.shadingNormal);
    const float3 specularReflections = DDGISampleLuminance(u_ddgiParams, u_probesIlluminanceTexture, u_probesDataSampler, u_probesHitDistanceTexture, u_probesDataSampler, specularSampleParams);
    luminance += specularReflections * (surface.specularColor * integratedBRDF.x + integratedBRDF.y);
#endif // DS_DDGIDS

    return luminance;
}

#endif // GlobalLightsDS
