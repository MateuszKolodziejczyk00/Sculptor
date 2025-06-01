#ifndef CLOUDSCAPE_RAYMARCHER_HLSLI
#define CLOUDSCAPE_RAYMARCHER_HLSLI

#include "Utils/Shapes.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Atmosphere/VolumetricClouds/CloudSampler.hlsli"


static float globalCloudsExtinction = 0.07f;

struct CloudscapeRaymarchResult
{
    float3 inScattering;
    float  transmittance;

    float cloudDepth;

    static CloudscapeRaymarchResult Create()
    {
        CloudscapeRaymarchResult val;
        val.inScattering  = 0.f;
        val.transmittance = 1.f;
        val.cloudDepth    = 0.f;

        return val;
    }
};


struct CloudscapeRaymarchParams
{
    Ray ray;
    float samplesNum;

    float noise;

    float3 ambient;

    static CloudscapeRaymarchParams Create()
    {
        CloudscapeRaymarchParams params;

        params.samplesNum        = 128.f;
        params.noise             = 1.f;
        params.ambient           = 0.f;

        return params;
    }
};


float CloudPhaseFunction(in float cosTheta, in float phaseMultiplier)
{
    const float eccentricity = 0.6f;
    const float silverSpread = 0.03f;
    const float silverIntensity = 1.0f;

#if 0
    const float blend = 0.5f;
    return lerp(HenyeyGreensteinPhaseFunction(0.8f * phaseMultiplier, cosTheta), HenyeyGreensteinPhaseFunction(-0.2f * phaseMultiplier, cosTheta), blend);
#else
    return max(HenyeyGreensteinPhaseFunction(eccentricity * phaseMultiplier, cosTheta), silverIntensity * HenyeyGreensteinPhaseFunction(phaseMultiplier * (0.99f - silverSpread), cosTheta));
#endif
}


float3 SampleSunTransmittanceAtLocation(in float3 location, in float3 lightDirection)
{
    const float3 bias = float3(0.f, 0.f, 200.f);
    const float3 sampleLocationInAtmosphere = GetLocationInAtmosphere(u_atmosphereConstants, location + bias);

    const float3 sunTransmittance = GetTransmittanceFromLUT(u_atmosphereConstants, u_transmittanceLUT, u_linearClampSampler, sampleLocationInAtmosphere, -lightDirection);

    return sunTransmittance;
}


float ComputeLocalVisibilityTerm(in CloudsSampler cs, in float3 origin, in float3 direction, in float LdotN, in float sequence)
{
    const uint samplesNum = 12u;

    float transmittance = 1.f;
    float powder = 1.f;

    const float stepFactor = 1.8f;
    float currentStep = 4.f + sequence * 4.f;

    float currT = 0.f;
    
    for(uint sampleIdx = 0.0; sampleIdx < samplesNum; ++sampleIdx)
    {
        const float newT = currT + currentStep;
        const float dt = newT - currT;
        currT = newT;

        const float3 sampleLocation = origin + direction * newT;

        int detailLevel = 2;

        const float cloudDensity = cs.SampleDensity(sampleLocation, detailLevel).x;

        const float cloudExtinction = cloudDensity * globalCloudsExtinction;
        transmittance *= max(0.000001f, exp(-dt * cloudExtinction));
        powder        *= max(0.000001f, exp(-2.f * dt * cloudExtinction));

        currentStep *= stepFactor;
    }

#define USE_POWDER 0

#if USE_POWDER
    return Remap(LdotN, -1.f, 1.f, 2.f * (1.f - powder), 1.f) * transmittance;
#else
    return transmittance;
#endif
}


float ComputeGroundIntersectionDist(in Ray viewRay)
{
    viewRay.origin = GetLocationInAtmosphere(u_atmosphereConstants, viewRay.origin);

	const Sphere groundSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereConstants.groundRadiusMM);

    const IntersectionResult intersection = viewRay.IntersectSphere(groundSphere);

    return intersection.IsValid() ? intersection.GetTime() * 1000000.f : 99999999.f;
}


float3 ComputeGroundLuminance()
{
    const float3 groundAlbedo = 0.4f;

    const float3 groundNormal = UP_VECTOR;

    const DirectionalLightGPUData directionalLight = u_cloudscapeConstants.mainDirectionalLight;

    const float NoL = dot(groundNormal, -directionalLight.direction);

    return Diffuse_Lambert(groundAlbedo) * NoL * directionalLight.illuminance;
}


bool ComputeRaymarchSegment(in Ray viewRay, out float2 segment)
{
    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Sphere cloudsAtmosphereInnerSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereInnerRadius);
    const Sphere cloudsAtmosphereOuterSphere = Sphere::Create(cloudscape.cloudsAtmosphereCenter, cloudscape.cloudsAtmosphereOuterRadius);

    const IntersectionResult innerIntersection = viewRay.IntersectSphere(cloudsAtmosphereInnerSphere);
    const IntersectionResult outerIntersection = viewRay.IntersectSphere(cloudsAtmosphereOuterSphere);

    const bool isInInner = cloudsAtmosphereInnerSphere.IsInside(viewRay.origin);
    const bool isInOuter = cloudsAtmosphereOuterSphere.IsInside(viewRay.origin);

    const float groundIntersectionDist = ComputeGroundIntersectionDist(viewRay);

    if(isInInner)
    {
        segment.x = min(groundIntersectionDist, innerIntersection.GetTime());
        segment.y = min(groundIntersectionDist, outerIntersection.GetTime());
    }
    else if(isInOuter)
    {
        segment.x = 0.f;
        segment.y = min(min(groundIntersectionDist, 
                            innerIntersection.IsValid() ? innerIntersection.GetTime() : FLOAT_MAX),
                            outerIntersection.IsValid() ? outerIntersection.GetTime() : FLOAT_MAX);

        segment.y = min(segment.y, 40000.f);
    }
    else
    {
        if(outerIntersection.IsValid())
        {
            segment.x = outerIntersection.GetTime();
            segment.y = min(groundIntersectionDist, innerIntersection.IsValid() ? innerIntersection.GetTime() : FLOAT_MAX);
        }
        else
        {
            segment = 0.f;
        }
    }

    return !AreNearlyEqual(segment.x, segment.y);
}


float RaymarchCloudscapeTransmittance(in CloudscapeRaymarchParams params)
{
    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Ray ray = params.ray;

    float2 raymarchSegment;
    const bool shouldTrace = ComputeRaymarchSegment(ray, OUT raymarchSegment);

    if(!shouldTrace)
    {
        return 1.f;
    }

    CloudsSampler cs = CreateCloudscapeSampler();

    const float dt = (raymarchSegment.y - raymarchSegment.x) / params.samplesNum;

    const float rayStart = params.noise;

    float transmittance = 1.f;

    for (float sampleIdx = rayStart; sampleIdx <= params.samplesNum; sampleIdx += 1.f)
    {
        const float rayT = raymarchSegment.x + sampleIdx * dt;
        const float3 sampleLocation = ray.origin + ray.direction * rayT;

        float2 sampledCloud = cs.SampleDensity(sampleLocation, CLOUDS_HIGHEST_DETAIL_LEVEL);

        const float cloudDensity = sampledCloud.x;

        if(cloudDensity == 0.f)
        {
            continue;
        }

        const float cloudExtinction = cloudDensity * globalCloudsExtinction;

        const float msTransmittance = max(0.000001f, exp(-dt * cloudExtinction));

        transmittance *= msTransmittance;

        if(transmittance < 0.001f)
        {
           transmittance = 0.0f;
           break;
        }
    }

    return transmittance;
}


CloudscapeRaymarchResult RaymarchCloudscape(in CloudscapeRaymarchParams params)
{
    CloudscapeRaymarchResult result = CloudscapeRaymarchResult::Create();

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Ray ray = params.ray;

    float2 raymarchSegment;
    const bool shouldTrace = ComputeRaymarchSegment(ray, OUT raymarchSegment);

    if(!shouldTrace)
    {
        return result;
    }

    CloudsSampler cs = CreateCloudscapeSampler();

    float dt = (raymarchSegment.y - raymarchSegment.x) / params.samplesNum;

    const float rayStart = params.noise;

    const DirectionalLightGPUData directionalLight = cloudscape.mainDirectionalLight;
    const float cosTheta = dot(-ray.direction, directionalLight.direction);

    const float octaveMul = 0.8f;

    #define MS_OCTAVES_NUM 7
    float phaseFunctions[MS_OCTAVES_NUM];
    float currentPhaseMultiplier = 1.f;
    for(uint octaveIdx = 0u; octaveIdx < MS_OCTAVES_NUM; ++octaveIdx)
    {
        phaseFunctions[octaveIdx] = CloudPhaseFunction(cosTheta, currentPhaseMultiplier);
        currentPhaseMultiplier *= octaveMul;
    }

    float transmittance[MS_OCTAVES_NUM];
    float3 inScattering = 0.f;

    for(uint octaveIdx = 0u; octaveIdx < MS_OCTAVES_NUM; ++octaveIdx)
    {
        transmittance[octaveIdx] = 1.f;
    }

    const float powderAlpha = -cosTheta * 0.5f + 0.5f;

    float cloudDepth = 0.f;
    float cloudDepthWeightSum = 0.f;

    const float3 groundAmbient = ComputeGroundLuminance();

    for (float sampleIdx = rayStart; sampleIdx <= params.samplesNum; sampleIdx += 1.f)
    {
        const float rayT = raymarchSegment.x + sampleIdx * dt;
        const float3 sampleLocation = ray.origin + ray.direction * rayT;

        float2 sampledCloud = cs.SampleDensity(sampleLocation, CLOUDS_HIGHEST_DETAIL_LEVEL);

        const float cloudDensity = sampledCloud.x;
        const float skyVisibility = sampledCloud.y;

        if(cloudDensity == 0.f)
        {
            continue;
        }

        float3 sampleInScattering = 0.f;
        const float3 sunTransmittance = SampleSunTransmittanceAtLocation(sampleLocation, directionalLight.direction);

        const float localVisibility = ComputeLocalVisibilityTerm(cs, sampleLocation, -directionalLight.direction, cosTheta, frac(params.noise + sampleIdx * SPT_GOLDEN_RATIO));

        const float  cloudExtinction   = cloudDensity * globalCloudsExtinction;
        const float3 cloudAlbedo = 0.98f;
        const float3 cloudInScattering = cloudAlbedo * cloudExtinction;

        const float3 ambient = (skyVisibility * params.ambient + (1.f - skyVisibility) * groundAmbient) / MS_OCTAVES_NUM;

        float currentOctaveMultiplier = 1.f;
        for(uint octaveIdx = 0u; octaveIdx < MS_OCTAVES_NUM; ++octaveIdx)
        {
            const float octaveExtinction = currentOctaveMultiplier;
            const float3 octaveScattering = cloudInScattering * currentOctaveMultiplier;

            const float msExtinction = cloudExtinction * octaveExtinction;

            float3 octaveInScattering = localVisibility * phaseFunctions[octaveIdx] * sunTransmittance * directionalLight.outerSpaceIlluminance + ambient;
            octaveInScattering *= octaveScattering;

            float msTransmittance = max(0.000001f, exp(-dt * msExtinction));

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
        cloudDepth = cloudDepth / max(cloudDepthWeightSum, 0.0001f);
    }
    else
    {
        cloudDepth = 0.f;
    }

    result.inScattering  = inScattering;
    result.transmittance = transmittance[0];
    result.cloudDepth    = cloudDepth;

    return result;
}

#endif // CLOUDSCAPE_RAYMARCHER_HLSLI