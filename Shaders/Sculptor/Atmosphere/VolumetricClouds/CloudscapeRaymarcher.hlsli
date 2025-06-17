#ifndef CLOUDSCAPE_RAYMARCHER_HLSLI
#define CLOUDSCAPE_RAYMARCHER_HLSLI

#include "Utils/Shapes.hlsli"
#include "Utils/Wave.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Atmosphere/VolumetricClouds/CloudSampler.hlsli"


#define PROBE_CLODUD_SCATTERING_OCTAVES_NUM 3
#define MAIN_VIEW_CLODUD_SCATTERING_OCTAVES_NUM 7

#define CLOUD_OCTAVE_MUL 0.8f


static const float globalCloudsExtinction = 0.045f;
static const float3 cloudAlbedo = 0.98f;
    

struct CloudscapeRaymarchResult
{
    float3 inScattering;
    float  transmittance;

    float cloudDepth;

    bool wasTraced;

    static CloudscapeRaymarchResult Create()
    {
        CloudscapeRaymarchResult val;
        val.inScattering  = 0.f;
        val.transmittance = 1.f;
        val.cloudDepth    = 0.f;
        val.wasTraced     = false;

        return val;
    }
};


struct CloudscapeRaymarchParams
{
    Ray ray;
    float samplesNum;

    float noise;

    float3 ambient;

    float maxVisibleDepth;

    uint detailLevel;

    static CloudscapeRaymarchParams Create()
    {
        CloudscapeRaymarchParams params;

        params.samplesNum      = 128.f;
        params.noise           = 1.f;
        params.ambient         = 0.f;
        params.maxVisibleDepth = -1.f;
        params.detailLevel     = CLOUDS_HIGHEST_DETAIL_LEVEL;

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
    const float3 sampleLocationInAtmosphere = GetLocationInAtmosphere(u_atmosphereConstants, location * float3(10.f, 10.f, 1.f) + bias);

    const float3 sunTransmittance = GetTransmittanceFromLUT(u_atmosphereConstants, u_transmittanceLUT, u_linearClampSampler, sampleLocationInAtmosphere, -lightDirection);

    return sunTransmittance;
}


float ComputeLocalVisibilityTerm(in CloudsSampler cs, in float3 origin, in float3 direction, in float LdotN, in float sequence, in uint detailLevel)
{
    const uint samplesNum = detailLevel == CLOUDS_HIGHEST_DETAIL_LEVEL ? 12u : 8u;

    float transmittance = 1.f;
    float powder = 1.f;

    const float stepFactor = detailLevel == CLOUDS_HIGHEST_DETAIL_LEVEL ? 1.8f : 2.f;
    float currentStep = 4.f + sequence * 4.f;

    float currT = 0.f;

#define USE_POWDER 0
    
    for(uint sampleIdx = 0.0; sampleIdx < samplesNum; ++sampleIdx)
    {
        const float newT = currT + currentStep;
        const float dt = newT - currT;
        currT = newT;

        const float3 sampleLocation = origin + direction * newT;

        const float cloudDensity = cs.SampleDensity(sampleLocation, detailLevel).x;

        const float cloudExtinction = cloudDensity * globalCloudsExtinction;
        transmittance *= max(0.000001f, exp(-dt * cloudExtinction));
#if USE_POWDER
        powder        *= max(0.000001f, exp(-2.f * dt * cloudExtinction));
#endif

        if(transmittance < 0.001f)
        {
            return 0.f;
        }

        currentStep *= stepFactor;
    }

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


bool ComputeRaymarchSegment(in Ray viewRay, in float maxVisibibleDepth, out float2 segment)
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

    if(maxVisibibleDepth > 0.f)
    {
        segment = min(segment, maxVisibibleDepth);
    }

    return !AreNearlyEqual(segment.x, segment.y);
}


float RaymarchCloudscapeTransmittance(in CloudscapeRaymarchParams params)
{
    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Ray ray = params.ray;

    float2 raymarchSegment;
    const bool shouldTrace = ComputeRaymarchSegment(ray, params.maxVisibleDepth, OUT raymarchSegment);

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

        float2 sampledCloud = cs.SampleDensity(sampleLocation, params.detailLevel);

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


template<uint octavesNum>
CloudscapeRaymarchResult RaymarchCloudscape(in CloudscapeRaymarchParams params)
{
    CloudscapeRaymarchResult result = CloudscapeRaymarchResult::Create();

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Ray ray = params.ray;

    float2 raymarchSegment;
    const bool shouldTrace = ComputeRaymarchSegment(ray, params.maxVisibleDepth, OUT raymarchSegment);

    if(!shouldTrace)
    {
        return result;
    }

    result.wasTraced = true;

    CloudsSampler cs = CreateCloudscapeSampler();

    float dt = (raymarchSegment.y - raymarchSegment.x) / params.samplesNum;

    const float rayStart = params.noise;

    const DirectionalLightGPUData directionalLight = cloudscape.mainDirectionalLight;
    const float cosTheta = dot(-ray.direction, directionalLight.direction);

    float phaseFunctions[octavesNum];
    float currentPhaseMultiplier = 1.f;
    
    for(uint octaveIdx = 0u; octaveIdx < octavesNum; ++octaveIdx)
    {
        phaseFunctions[octaveIdx] = CloudPhaseFunction(cosTheta, currentPhaseMultiplier);
        currentPhaseMultiplier *= CLOUD_OCTAVE_MUL;
    }

    float transmittance[octavesNum];
    float3 inScattering = 0.f;

    for(uint octaveIdx = 0u; octaveIdx < octavesNum; ++octaveIdx)
    {
        transmittance[octaveIdx] = 1.f;
    }

    const float powderAlpha = -cosTheta * 0.5f + 0.5f;

    float cloudDepth = 0.f;
    float cloudDepthWeightSum = 0.f;

    for (float sampleIdx = rayStart; sampleIdx <= params.samplesNum; sampleIdx += 1.f)
    {
        const float rayT = raymarchSegment.x + sampleIdx * dt;
        const float3 sampleLocation = ray.origin + ray.direction * rayT;

        float2 sampledCloud = cs.SampleDensity(sampleLocation, params.detailLevel);

        const float cloudDensity = sampledCloud.x;
        const float skyVisibility = sampledCloud.y;

        if(cloudDensity == 0.f)
        {
            continue;
        }

        float3 sampleInScattering = 0.f;
        const float3 sunTransmittance = SampleSunTransmittanceAtLocation(sampleLocation, directionalLight.direction);

        const float localVisibility = ComputeLocalVisibilityTerm(cs, sampleLocation, -directionalLight.direction, cosTheta, frac(params.noise + sampleIdx * SPT_GOLDEN_RATIO), params.detailLevel);

        const float  cloudExtinction   = cloudDensity * globalCloudsExtinction;
        const float3 cloudInScattering = cloudAlbedo * cloudExtinction;

        const float3 ambient = (skyVisibility * params.ambient) / octavesNum;

        float currentOctaveMultiplier = 1.f;
        for(uint octaveIdx = 0u; octaveIdx < octavesNum; ++octaveIdx)
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

            currentOctaveMultiplier *= CLOUD_OCTAVE_MUL;
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
        cloudDepth = -1.f;
    }

    result.inScattering  = inScattering;
    result.transmittance = transmittance[0];
    result.cloudDepth    = cloudDepth;

    return result;
}


#define MAX_CLOUDSCAPE_SAMPLES 64 
groupshared float gs_cachedSamplesT[MAX_CLOUDSCAPE_SAMPLES];
groupshared half2 gs_cachedSamplesCloud[MAX_CLOUDSCAPE_SAMPLES];


template<uint octavesNum>
CloudscapeRaymarchResult WaveRaymarchCloudscape(in CloudscapeRaymarchParams params)
{
    CloudscapeRaymarchResult result = CloudscapeRaymarchResult::Create();

    const CloudscapeConstants cloudscape = u_cloudscapeConstants;

    const Ray ray = params.ray;

    float2 raymarchSegment;
    const bool shouldTrace = ComputeRaymarchSegment(ray, params.maxVisibleDepth, OUT raymarchSegment);

    if(!shouldTrace)
    {
        return result;
    }

    result.wasTraced = true;

    CloudsSampler cs = CreateCloudscapeSampler();

    float dt = (raymarchSegment.y - raymarchSegment.x) / params.samplesNum;

    uint validSamplesNum = 0u;
    for (uint sampleIdx = WaveGetLaneIndex(); sampleIdx < params.samplesNum; sampleIdx += WaveGetLaneCount())
    {
        const float sampleT = float(sampleIdx) + params.noise;

        const float rayT = raymarchSegment.x + sampleT * dt;
        const float3 sampleLocation = ray.origin + ray.direction * rayT;

        const float2 sampledCloud = cs.SampleDensity(sampleLocation, params.detailLevel);
        const float cloudDensity = sampledCloud.x;

        const bool isSampleValid = cloudDensity > 0.f;

        const uint2 meshletsVisibleBallot = WaveActiveBallot(isSampleValid).xy;

        if(isSampleValid)
        {
            const uint sampleIdx = validSamplesNum + GetCompactedIndex(meshletsVisibleBallot, WaveGetLaneIndex());

            gs_cachedSamplesT[sampleIdx] = rayT;
            gs_cachedSamplesCloud[sampleIdx] = half2(sampledCloud);
        }

        validSamplesNum += countbits(meshletsVisibleBallot.x) + countbits(meshletsVisibleBallot.y);
    }

    if(validSamplesNum == 0u)
    {
        return result;
    }

    const DirectionalLightGPUData directionalLight = cloudscape.mainDirectionalLight;
    const float cosTheta = dot(-ray.direction, directionalLight.direction);

    float phaseFunctions[octavesNum];
    float currentPhaseMultiplier = 1.f;
    for(uint octaveIdx = 0u; octaveIdx < octavesNum; ++octaveIdx)
    {
        phaseFunctions[octaveIdx] = CloudPhaseFunction(cosTheta, currentPhaseMultiplier);
        currentPhaseMultiplier *= CLOUD_OCTAVE_MUL;
    }

    float transmittance[octavesNum];
    float3 inScattering = 0.f;

    for(uint octaveIdx = 0u; octaveIdx < octavesNum; ++octaveIdx)
    {
        transmittance[octaveIdx] = 1.f;
    }

    const float3 ambient = params.ambient / octavesNum;

    AllMemoryBarrierWithGroupSync();

    [unroll(4)]
    for (uint sampleIdx = WaveGetLaneIndex(); sampleIdx < validSamplesNum; sampleIdx += WaveGetLaneCount())
    {
        const float sampleT = float(sampleIdx) + params.noise;

        const float rayT = gs_cachedSamplesT[sampleIdx];
        const float3 sampleLocation = ray.origin + ray.direction * rayT;

        const float2 sampledCloud = gs_cachedSamplesCloud[sampleIdx];

        const float cloudDensity = sampledCloud.x;
        const float skyVisibility = sampledCloud.y;

        float3 sampleInScattering = 0.f;
        const float3 sunTransmittance = SampleSunTransmittanceAtLocation(sampleLocation, directionalLight.direction);

        const float localVisibility = ComputeLocalVisibilityTerm(cs, sampleLocation, -directionalLight.direction, cosTheta, frac(params.noise + sampleT * SPT_GOLDEN_RATIO), params.detailLevel);

        const float  cloudExtinction   = cloudDensity * globalCloudsExtinction;
        const float3 cloudInScattering = cloudAlbedo * cloudExtinction;

        const float3 ambientWithVisiblity = skyVisibility * ambient;

        float currentOctaveMultiplier = 1.f;

        [unroll]
        for(uint octaveIdx = 0u; octaveIdx < octavesNum; ++octaveIdx)
        {
            const float octaveExtinction = currentOctaveMultiplier;
            const float3 octaveScattering = cloudInScattering * currentOctaveMultiplier;

            const float msExtinction = cloudExtinction * octaveExtinction;

            float3 octaveInScattering = localVisibility * phaseFunctions[octaveIdx] * sunTransmittance * directionalLight.outerSpaceIlluminance + ambientWithVisiblity;
            octaveInScattering *= octaveScattering;

            float msTransmittance = max(0.000001f, exp(-dt * msExtinction));

            const float accumulatedTransmittancePrefix = transmittance[octaveIdx] * WavePrefixProduct(msTransmittance);

            const float3 inScatteringIntegral = (octaveInScattering - octaveInScattering * msTransmittance) / max(msExtinction, 0.000001f);

            const float3 deltaScattering = accumulatedTransmittancePrefix * inScatteringIntegral;

            inScattering += deltaScattering;
            transmittance[octaveIdx] = WaveReadLaneAt(accumulatedTransmittancePrefix * msTransmittance, WaveActiveMax(WaveGetLaneIndex()));

            currentOctaveMultiplier *= CLOUD_OCTAVE_MUL;

            transmittance[octaveIdx] = accumulatedTransmittancePrefix * msTransmittance;
        }

        if(transmittance[0] < 0.001f)
        {
           transmittance[0] = 0.0f;
           break;
        }
    }

    result.inScattering  = WaveActiveSum(inScattering);
    result.transmittance = WaveActiveMin(transmittance[0]);

    return result;
}

#endif // CLOUDSCAPE_RAYMARCHER_HLSLI