#include "SculptorShader.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(ApplyAtmosphereDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyAtmosphereCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_luminanceTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float depth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0);

        if(depth < 0.00001f)
        {
            const float3 rayDirection = ComputeViewRayDirectionWS(u_sceneView, uv);

            const float3 viewLocation = GetLocationInAtmosphere(u_atmosphereParams, u_sceneView.viewLocation);

            float3 skyLuminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, viewLocation, rayDirection);

            for (int lightIdx = 0; lightIdx < u_atmosphereParams.directionalLightsNum; ++lightIdx)
            {
                const DirectionalLightGPUData directionalLight = u_directionalLights[lightIdx];
                const float3 lightDirection = -directionalLight.direction;

                const float rayLightDot = dot(rayDirection, lightDirection);
                const float minRayLightDot = 0.9998f;
                if (rayLightDot > minRayLightDot)
                {
                    const float alpha = (rayLightDot - minRayLightDot) / (1.f - minRayLightDot);
                    const float3 transmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, viewLocation, rayDirection);
                    skyLuminance += directionalLight.illuminance * alpha * transmittance * directionalLight.sunDiskIntensity;
                }
            }

            skyLuminance *= u_viewRenderingParams.preExposure;
            
            u_luminanceTexture[pixel] = skyLuminance;
        }
    }
}
