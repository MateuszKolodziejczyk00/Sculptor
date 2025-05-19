#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricCloudsMainViewDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(CloudscapeDS, 2)]]


#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/Shapes.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Atmosphere/VolumetricClouds/CloudSampler.hlsli"

static float globalCloudsExtinction = 0.006f;

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float CloudPhaseFunction(in float cosTheta, in float phaseMultiplier)
{
    const float eccentricity = 0.6f;
    const float silverSpread = 0.04f;
    const float silverIntensity = 1.0f;

#if 0
    const float blend = 0.5f;
    return lerp(HenyeyGreensteinPhaseFunction(0.8f * phaseMultiplier, cosTheta), HenyeyGreensteinPhaseFunction(-0.3f * phaseMultiplier, cosTheta), blend);
#else
    return max(HenyeyGreensteinPhaseFunction(eccentricity * phaseMultiplier, cosTheta), silverIntensity * phaseMultiplier * HenyeyGreensteinPhaseFunction(phaseMultiplier * (0.99f - silverSpread), cosTheta));
#endif
}


float3 SampleSunTransmittanceAtLocation(in float3 location, in float3 lightDirection, in Sphere cloudsAtmosphereInnerSphere)
{
#if 0
    const float distFromCenter = length(location - cloudsAtmosphereInnerSphere.center);
    const float3 dirFromCenter = (location - cloudsAtmosphereInnerSphere.center) / distFromCenter;

    const float height = 1500.f + distFromCenter - cloudsAtmosphereInnerSphere.radius;

    const float3 sampleLocationInAtmosphere = dirFromCenter * (u_atmosphereConstants.groundRadiusMM + height * 0.000001f);
#else

    const float3 sampleLocationInAtmosphere = GetLocationInAtmosphere(u_atmosphereConstants, location);

#endif
    const float3 sunTransmittance = GetTransmittanceFromLUT(u_atmosphereConstants, u_transmittanceLUT, u_linearClampSampler, sampleLocationInAtmosphere, -lightDirection);

    return sunTransmittance;
}


float ComputeLocalVisibilityTerm(in CloudsSampler cs, in float3 origin, in float3 direction, in float LdotN, in float sequence)
{
    const uint samplesNum = 12u;

    float transmittance = 1.f;
    float powder = 1.f;

    const float stepFactor = 1.8f;
    float currentStep = 3.f + sequence * 3.f;

    float currT = 0.f;
    
    for(uint sampleIdx = 0.0; sampleIdx < samplesNum; ++sampleIdx)
    {
        const float newT = currT + currentStep;
        const float dt = newT - currT;
        currT = newT;

        const float3 sampleLocation = origin + direction * newT;

        int detailLevel = 2;
        if(transmittance < 0.4f)
        {
            detailLevel = 0;
        }
        else if(transmittance < 0.9f)
        {
            detailLevel = 1;
        }

        const float cloudDensity = cs.SampleDensity(sampleLocation, detailLevel).x;

        const float cloudExtinction = cloudDensity * globalCloudsExtinction;
        transmittance *= max(0.000001f, exp(-dt * cloudExtinction));
        powder        *= max(0.000001f, exp(-2.f * dt * cloudExtinction));

        currentStep *= stepFactor;
    }

#define USE_POWDER 0

#if USE_POWDER
    return Remap(LdotN, -1.f, 1.f, (1.f - powder), 1.f) * transmittance;
#else
    return transmittance;
#endif
}


bool DoesIntersectGround(Ray viewRay)
{
    viewRay.origin = GetLocationInAtmosphere(u_atmosphereConstants, viewRay.origin);

	const Sphere groundSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereConstants.groundRadiusMM);

    return viewRay.IntersectSphere(groundSphere).IsValid();
}


[numthreads(8, 8, 1)]
void RenderVolumetricCloudsMainViewCS(CS_INPUT input)
{
    const uint3 coords = uint3(input.globalID.xy, 0u);

    const float depth = u_depth.Load(coords);

    if(depth > 0.f)
    {
        u_rwClouds[coords.xy] = float4(0.f, 0.f, 0.f, 1.f);
        return;
    }

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Sphere cloudsAtmosphereInnerSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereInnerRadius);
    const Sphere cloudsAtmosphereOuterSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereOuterRadius + 3000.f);
    
    const float2 uv = (coords.xy + 0.5f) * u_passConstants.rcpResolution;

    const Ray viewRay = CreateViewRayWS(u_sceneView, uv);

    if(DoesIntersectGround(viewRay))
    {
        u_rwClouds[coords.xy] = float4(0.f, 0.f, 0.f, 1.f);
        return;
    }

    const IntersectionResult innerIntersection = viewRay.IntersectSphere(cloudsAtmosphereInnerSphere);
    const IntersectionResult outerIntersection = viewRay.IntersectSphere(cloudsAtmosphereOuterSphere);

    const float3 innerAtmosphereLocation = innerIntersection.IsValid() ? viewRay.GetIntersectionLocation(innerIntersection) : viewRay.origin;
    const float3 outerAtmosphereLocation = outerIntersection.IsValid() ? viewRay.GetIntersectionLocation(outerIntersection) : viewRay.origin;

    CloudsSampler cs = CreateCloudscapeSampler();

    const float samplesNum = 128.f;

    float dt = distance(innerAtmosphereLocation, outerAtmosphereLocation) / samplesNum;

    const float blueNoise = frac(u_blueNoise256.Load(coords & 255u) + (u_passConstants.frameIdx & 31u) * SPT_GOLDEN_RATIO);
    const float rayStart = blueNoise;

    const DirectionalLightGPUData directionalLight = u_directionalLights[0];
    const float cosTheta = dot(-viewRay.direction, directionalLight.direction);

    const float octaveMul = 0.8f;

    #define MS_OCTAVES_NUM 3
    float phaseFunctions[MS_OCTAVES_NUM];
    float currentPhaseMultiplier = 1.f;
    for(uint octaveIdx = 0u; octaveIdx < MS_OCTAVES_NUM; ++octaveIdx)
    {
        phaseFunctions[octaveIdx] = CloudPhaseFunction(cosTheta, currentPhaseMultiplier);
        currentPhaseMultiplier *= octaveMul;
    }

    float transmittance[MS_OCTAVES_NUM];
    float powder[MS_OCTAVES_NUM];
    float3 inScattering = 0.f;

    for(uint octaveIdx = 0u; octaveIdx < MS_OCTAVES_NUM; ++octaveIdx)
    {
        transmittance[octaveIdx] = 1.f;
    }

    const float3 skyAvgLuminance = u_skyProbe.Load(uint3(0u, 0u, 0u));

    float cloudDepth = 0.f;
    float cloudDepthWeightSum = 0.f;

    for (float sampleIdx = rayStart; sampleIdx <= samplesNum; sampleIdx += 1.f)
    {
        const float rayT = sampleIdx * dt;
        const float3 sampleLocation = innerAtmosphereLocation + viewRay.direction * rayT;

        const float2 sampledCloud = cs.SampleDensity(sampleLocation, CLOUDS_HIGHEST_DETAIL_LEVEL);
        const float cloudDensity = sampledCloud.x;
        const float h = sampledCloud.y;

        if(cloudDensity == 0.f)
        {
            continue;
        }

        float3 sampleInScattering = 0.f;
        const float3 sunTransmittance = SampleSunTransmittanceAtLocation(sampleLocation, directionalLight.direction, cloudsAtmosphereInnerSphere);

        const float localVisibility = ComputeLocalVisibilityTerm(cs, sampleLocation, -directionalLight.direction, cosTheta, frac(blueNoise + sampleIdx * SPT_GOLDEN_RATIO));

        const float  cloudExtinction   = cloudDensity * globalCloudsExtinction;
        const float3 cloudAlbedo = 0.98f;
        const float3 cloudInScattering = cloudAlbedo * cloudExtinction;

        const float3 ambient = skyAvgLuminance * lerp(0.7f, 1.3f, h);

        float currentOctaveMultiplier = 1.f;
        for(uint octaveIdx = 0u; octaveIdx < MS_OCTAVES_NUM; ++octaveIdx)
        {
            const float octaveExtinction = currentOctaveMultiplier;
            const float3 octaveScattering = cloudInScattering * currentOctaveMultiplier;

            const float msExtinction = cloudExtinction * octaveExtinction;

            const float3 octaveInScattering = octaveScattering * (localVisibility * phaseFunctions[octaveIdx] * sunTransmittance * directionalLight.outerSpaceIlluminance + ambient);

            const float powderStrength = 2.f; // guerilla default is 2.f

            const float msTransmittance = max(0.000001f, exp(-dt * msExtinction));

            const float octaveTransmittance = transmittance[octaveIdx];

            const float3 inScatteringIntegral = (octaveInScattering - octaveInScattering * msTransmittance) / max(msExtinction, 0.000001f);

            const float3 deltaScattering = octaveTransmittance * inScatteringIntegral;

            inScattering += deltaScattering;

            transmittance[octaveIdx] *= msTransmittance;

            if(octaveIdx == 0u)
            {
                const float weight = (1.f - msTransmittance);
                cloudDepth += weight * rayT;
                cloudDepthWeightSum += weight;
            }

            currentOctaveMultiplier *= octaveMul;
        }

        if(transmittance[0] < 0.001f)
        {
           transmittance[0] = 0.0f;
           break;
        }
    }


    if(cloudDepthWeightSum > 0.f)
    {
        cloudDepth = innerIntersection.GetTime() + cloudDepth / max(cloudDepthWeightSum, 0.0001f);
        cloudDepth = ComputeProjectionDepth(cloudDepth, u_sceneView);
    }
    else
    {
        cloudDepth = 0.f;
    }

    u_rwCloudsDepth[coords.xy] = cloudDepth;

    const float3 exposedInScattering = LuminanceToExposedLuminance(inScattering);

    u_rwClouds[coords.xy] = float4(exposedInScattering, transmittance[0]);
}