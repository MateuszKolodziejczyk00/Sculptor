#ifndef CLOUD_SAMPLER_HLSLI
#define CLOUD_SAMPLER_HLSLI

struct CloudsSamplerParams
{
    Texture3D<float> baseShapeNoise;
    SamplerState     baseShapeSampler;

    Texture3D<float4> detailShapeNoise;
    SamplerState      detailShapeSampler;

    Texture2D<float3> weather;
    SamplerState      weatherSampler;

    Texture2D<float3> curlNoise;
    SamplerState      curlNoiseSampler;

    float baseShapeScale;
    float detailShapeScale;
    float weatherScale;

    float cloudsMinHeight;
    float cloudsMaxHeight;

    float globalDensity;
    float globalCoverage;

    float3 atmosphereCenter;

    float time;
};


#define CLOUDS_HIGHEST_DETAIL_LEVEL 2


struct CloudsSampler
{
    CloudsSamplerParams params;

    static CloudsSampler Create(in CloudsSamplerParams inParams)
    {
        CloudsSampler s;
        s.params = inParams;
        return s;
    }

    float ComputeHeightAlphaAtLocation(in float3 location)
    {
        #if 0
        const float h = location.z;
        #else
        const float h = distance(params.atmosphereCenter, location) + params.atmosphereCenter.z;
        #endif
        return saturate((h - params.cloudsMinHeight) / (params.cloudsMaxHeight - params.cloudsMinHeight));
    }

    float SampleBaseShapeNoise(in float3 location)
    {
        const float3 uvw = location * params.baseShapeScale;
        return params.baseShapeNoise.Sample(params.baseShapeSampler, uvw).x;
    }
     
    float4 SampleDetailShapeNoise(in float3 location)
    {
        return params.detailShapeNoise.Sample(params.detailShapeSampler, location * params.detailShapeScale);
    }
     
    float3 SampleWeather(in float3 location)
    {
        return params.weather.SampleLevel(params.weatherSampler, location.xy * params.weatherScale + 0.5f, 0.f);
    }

    float3 SampleCurlNoise(in float3 location)
    {
        return params.curlNoise.SampleLevel(params.curlNoiseSampler, location.xy * 0.00006f, 0.f);
    }

    float3 ComputeDetailLocation(in float3 location)
    {
        const float h = ComputeHeightAlphaAtLocation(location);

        const float3 offset = float3((SampleCurlNoise(location) * 2.f - 1.f) * (25.f + (1.f - h) * 25.f));

        return location + offset;
    }

    float ComputeDetailNoise(in float3 location, in float h,  in float densityAltitude)
    {
        const float4 noise = SampleDetailShapeNoise(location);

        const float wispyNoise = lerp(noise.r, noise.g, saturate(densityAltitude));

        const float billowyTypeGradient = pow(densityAltitude, 0.25f);
        const float billowyNoise = lerp(noise.b * 0.3f, noise.a * 0.3f, billowyTypeGradient); // 0.3f is default

        const float noiseType = saturate((h - 0.2f) * 1.8f);
        float noiseComposite = saturate(lerp(wispyNoise, billowyNoise, noiseType)) * 0.999f;

        //if(detailed)
        //{
        //    float hhfNoise = saturate(lerp(1.f - pow(abs(abs(noise.g * 2.f - 1.f) * 2.f - 1.f), 4.f), pow(abs(abs(noise.a * 2.f - 1.f)* 2.f - 1.f), 2.f), noiseType));

        //    const float hhfNoiseDistanceRangeBlender = RemapClouds(h, 0.f, 0.1f, 0.9f, 1.f);
        //    noiseComposite = lerp(hhfNoise, noiseComposite, hhfNoiseDistanceRangeBlender);
        //}

        return noiseComposite;
    }

    float2 SampleDensity(in float3 location, in int detailLevel)
    {
        const float totalH = ComputeHeightAlphaAtLocation(location);

        float3 windDir = float3(1.f, 1.f, 0.f);
        location += totalH * windDir * 500.f;

        const float3 weatherMap = SampleWeather(location);

        float cloudsCoverage = 0.f;
        float cloudsHeight = 0.f;
        cloudsCoverage = max(cloudsCoverage, 1.f - length(location.xy) < 7000.f ? 0.9f : 0.f);
        cloudsHeight   = max(cloudsHeight, 1.f - pow(saturate(1.f - length(location.xy) / 7000.f), 0.3f));

        cloudsCoverage = max(weatherMap.r, cloudsCoverage);
        cloudsHeight = max(weatherMap.b, cloudsHeight);

        cloudsCoverage = max(weatherMap.r, 1.f);
        cloudsHeight = max(weatherMap.b, 1.f);

        if(cloudsCoverage < 0.001f)
        {
            return 0.f;
        }

        const float h = totalH / max(cloudsHeight, 0.0001f);

        const float shapeBottom = Remap<float>(h, 0.f, 0.07f, 0.f, 1.f);
        const float shapeTop    = cloudsHeight > 0.f ? Remap<float>(h, cloudsHeight * 0.2f, cloudsHeight, 1.f, 0.f) : 0.f;
        
        const float shapeAltitude = shapeBottom * shapeTop;

        const float densityBottom = Remap<float>(h, 0.f, 0.15f, 0.f, 1.f);
        const float densityTop = Remap<float>(h, 0.9f, 1.f, 1.f, 0.f);

        const float globalDensity = 1.f;
        const float weatherDensity = 1.f;

        const float densityAltitude = globalDensity * densityBottom * densityTop * weatherDensity * 2.f;

        const float baseCloudNoise = SampleBaseShapeNoise(location);

        float baseCloud = Remap<float>(params.globalCoverage * cloudsCoverage, min(baseCloudNoise, 0.9999f), 1.f, 0.f, 1.f) * shapeAltitude;

        if(baseCloud <= 0.001f)
        {
            return 0.f;
        }

        //const float anvilBias = 0.3f;
        //baseCloud = pow(baseCloud , Remap<float>(h, 0.7, 0.8, 1.f, lerp(1.f, 0.5, anvilBias)));

        float3 detailLocation = ComputeDetailLocation(location);
        
#if 0
        detailLocation += frac(params.time * windDir * 100.f * params.detailShapeScale) / params.detailShapeScale;
#endif


        float cloudDensity = baseCloud;

        if(detailLevel >= 1)
        {
            const float edgeDetail = Remap<float>(h, 0.3f, 0.6f, 1.2f, 0.7f);
            const float noiseComposite = ComputeDetailNoise(detailLocation, h, densityAltitude);
            cloudDensity = Remap<float>(baseCloud, min(edgeDetail * noiseComposite, 0.999f), 1.f, 0.f, 1.f);
        }

        if(detailLevel >= 2)
        {
            const float edgeDetail = Remap<float>(h, 0.3f, 0.6f, 0.4f, 0.15f);
            const float noiseComposite = ComputeDetailNoise(detailLocation * 3.f, h, densityAltitude);
            cloudDensity = Remap<float>(cloudDensity, edgeDetail * noiseComposite, 1.f, 0.f, 1.f);
        }

        cloudDensity *= densityAltitude;

        cloudDensity = pow(cloudDensity, lerp(1.5f, 0.6f, saturate(h * 3.f)));

        return float2(cloudDensity * params.globalDensity, h);
    }
};


#ifdef DS_CloudscapeDS
CloudsSampler CreateCloudscapeSampler()
{
    CloudsSamplerParams csParams;
    csParams.baseShapeNoise     = u_baseShapeNoise;
    csParams.baseShapeSampler   = u_linearRepeatSampler;
    csParams.detailShapeNoise   = u_detailShapeNoise;
    csParams.detailShapeSampler = u_linearRepeatSampler;
    csParams.weather            = u_weatherMap;
    csParams.weatherSampler     = u_linearRepeatSampler;
    csParams.curlNoise          = u_curlNoise;
    csParams.curlNoiseSampler   = u_linearRepeatSampler;
    csParams.baseShapeScale     = u_cloudscapeConstants.baseShapeNoiseScale;
    csParams.detailShapeScale   = u_cloudscapeConstants.detailShapeNoiseScale;
    csParams.weatherScale       = u_cloudscapeConstants.weatherMapScale;
    csParams.cloudsMinHeight    = u_cloudscapeConstants.cloudscapeInnerHeight;
    csParams.cloudsMaxHeight    = u_cloudscapeConstants.cloudscapeOuterHeight;
    csParams.globalDensity      = u_cloudscapeConstants.globalDensity;
    csParams.globalCoverage     = u_cloudscapeConstants.globalCoverage;
    csParams.atmosphereCenter   = u_cloudscapeConstants.cloudsAtmosphereCenter;
    csParams.time               = u_cloudscapeConstants.time;
    CloudsSampler cs = CloudsSampler::Create(csParams);

    return cs;
}
#endif

#endif // CLOUD_SAMPLER_HLSLI