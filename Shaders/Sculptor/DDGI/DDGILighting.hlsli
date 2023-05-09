#include "Lights/LightingUtils.hlsli"
#include "Lights/Shadows.hlsli"


struct DDGIShadowRayPayload
{
    bool isShadowed;
};


float3 CalcReflectedRadiance(ShadedSurface surface, float3 viewDir)
{
    float3 radiance = 0.f;

    // Directional Lights

    for (uint i = 0; i < u_lightsParams.directionalLightsNum; ++i)
    {
        const DirectionalLightGPUData directionalLight = u_directionalLights[i];

        const float3 lightIntensity = directionalLight.color * directionalLight.intensity;

        if (any(lightIntensity > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
        {
            DDGIShadowRayPayload payload = { true };

            RayDesc rayDesc;
            rayDesc.TMin        = 0.02f;
            rayDesc.TMax        = 50.f;
            rayDesc.Origin      = surface.location;
            rayDesc.Direction   = -directionalLight.direction;

            TraceRay(u_sceneAccelerationStructure,
                     RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                     0xFF,
                     0,
                     1,
                     1,
                     rayDesc,
                     payload);
            
            if (!payload.isShadowed)
            {
                radiance += CalcLighting(surface, -directionalLight.direction, viewDir, lightIntensity);
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
                const float3 lightIntensity = GetPointLightIntensityAtLocation(pointLight, surface.location);

                if(any(lightIntensity > 0.f))
                {
                    float visibility = 1.f;
                    if (pointLight.shadowMapFirstFaceIdx != IDX_NONE_32)
                    {
                        const float3 biasedLocation  = surface.location + surface.shadingNormal * 0.02f;
                        visibility = EvaluatePointLightShadowsAtLocation(surface.location, biasedLocation, pointLight.radius, pointLight.shadowMapFirstFaceIdx);
                    }
            
                    if (visibility > 0.f)
                    {
                        radiance += CalcLighting(surface, lightDir, viewDir, lightIntensity) * visibility;
                    }
                }
            }
        }
    }

    return radiance;
}
