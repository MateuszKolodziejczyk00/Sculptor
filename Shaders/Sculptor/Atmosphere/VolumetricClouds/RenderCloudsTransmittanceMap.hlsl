#include "SculptorShader.hlsli"

[[descriptor_set(RenderCloudsTransmittanceMapDS, 0)]]
[[descriptor_set(CloudscapeDS, 1)]]

#include "Utils/Shapes.hlsli"
#include "Atmosphere/VolumetricClouds/CloudSampler.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


static float globalCloudsExtinction = 0.006f;


[numthreads(8, 8, 1)]
void RenderCloudsTransmittanceMapCS(CS_INPUT input)
{
    const float2 uv = (input.globalID.xy + 0.5f) / 2048.f;

    const float4 ws = mul(u_constants.invViewProjectionMatrix, float4(2.f * uv - 1.f, 0.5f, 1.f));

    const float3 worldLocation = ws.xyz / ws.w;

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Sphere cloudsAtmosphereInnerSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereInnerRadius);
    const Sphere cloudsAtmosphereOuterSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereOuterRadius + 3000.f);

    float transmittance = 1.f;

    if(cloudsAtmosphereInnerSphere.IsInside(worldLocation))
    {
        const Ray ray = Ray::Create(worldLocation, u_constants.direction);

        const IntersectionResult innerIntersection = ray.IntersectSphere(cloudsAtmosphereInnerSphere);
        const IntersectionResult outerIntersection = ray.IntersectSphere(cloudsAtmosphereOuterSphere);

        const float3 innerAtmosphereLocation = ray.GetIntersectionLocation(innerIntersection);
        const float3 outerAtmosphereLocation = ray.GetIntersectionLocation(outerIntersection);

        CloudsSampler cs = CreateCloudscapeSampler();

        const float samplesNum = 128.f;

        const float dt = distance(innerAtmosphereLocation, outerAtmosphereLocation) / samplesNum;

        for (float sampleIdx = 1.f; sampleIdx <= samplesNum; sampleIdx += 1.f)
        {
            const float3 sampleLocation = innerAtmosphereLocation + sampleIdx * ray.direction * dt;

            const float2 sampledCloud = cs.SampleDensity(sampleLocation, CLOUDS_HIGHEST_DETAIL_LEVEL);
            const float cloudDensity = sampledCloud.x;
            const float h = sampledCloud.y;

            if(cloudDensity == 0.f)
            {
                continue;
            }

            const float  cloudExtinction   = cloudDensity * globalCloudsExtinction;
            const float sampleTransmittance = max(0.000001f, exp(-dt * cloudExtinction));

           transmittance *= sampleTransmittance;
        }
    }
    
    u_rwTransmittanceMap[input.globalID.xy] = transmittance;
}