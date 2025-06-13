#include "SculptorShader.hlsli"

[[descriptor_set(RenderCloudsTransmittanceMapDS, 0)]]
[[descriptor_set(CloudscapeDS, 1)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RenderCloudsTransmittanceMapCS(CS_INPUT input)
{
    const uint2 coords = u_constants.updateOffset + input.globalID.xy;

    const float2 uv = (coords + 0.5f) / 2048.f;

    const float4 ws = mul(u_constants.invViewProjectionMatrix, float4(2.f * uv - 1.f, 0.5f, 1.f));

    const float3 worldLocation = ws.xyz / ws.w;

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Sphere cloudsAtmosphereInnerSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereInnerRadius);
    const Sphere cloudsAtmosphereOuterSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereOuterRadius);

    float transmittance = 1.f;

    const float3 rayDir = u_constants.direction;
    float3 rayOrigin = worldLocation;
    
    if(!IsNearlyZero(rayDir.z))
    {
        const float2 dxy = rayDir.xy / rayDir.z;
        rayOrigin.xy -= dxy * rayOrigin.z;
        rayOrigin.z = 0.f;
    }

    const Ray ray = Ray::Create(rayOrigin, rayDir);

    CloudscapeRaymarchParams raymarchParams = CloudscapeRaymarchParams::Create();
    raymarchParams.ray                     = ray;
    raymarchParams.samplesNum              = 64.f;
    raymarchParams.detailLevel             = CLOUDS_DETAIL_PRESET_TRANSMITTANCE;
    transmittance = RaymarchCloudscapeTransmittance(raymarchParams);
    
    u_rwTransmittanceMap[coords] = transmittance;
}