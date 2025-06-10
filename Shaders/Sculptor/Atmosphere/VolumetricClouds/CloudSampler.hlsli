#ifndef CLOUD_SAMPLER_HLSLI
#define CLOUD_SAMPLER_HLSLI

struct CloudsSamplerParams
{
    Texture3D<float4> baseShapeNoise;
    SamplerState      baseShapeSampler;

    Texture3D<float4> detailShapeNoise;
    SamplerState      detailShapeSampler;

    Texture2D<float3> weather;
    SamplerState      weatherSampler;

    Texture2D<float3> curlNoise;
    SamplerState      curlNoiseSampler;

    Texture2D<float> densityLUT;
    SamplerState     densityLUTSampler;

    float baseShapeScale;
    float weatherScale;
    float cirrusMapScale;

    float detailShapeNoiseStrength0;
    float detailShapeNoiseScale0;
    float detailShapeNoiseStrength1;
    float detailShapeNoiseScale1;

    float curlNoiseScale;
    float curlMaxOffset;

    float cloudsMinHeight;
    float cloudsMaxHeight;

    float globalDensity;
    float globalCoverageOffset;
    float globalCloudsHeightOffset;

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
        const float3 uvw = location * params.baseShapeScale * float3(1.f, 1.f, 1.f);

        float4 noise =  params.baseShapeNoise.Sample(params.baseShapeSampler, uvw);

        const float lowFrequencyFBM = noise.g * 0.625f + noise.z * 0.25f + noise.w * 0.125f;

        const float baseCloud = RemapNoClamp<float>(noise.r, -(1.f - lowFrequencyFBM), 1.f, 0.f, 1.f);

        return baseCloud;
    }
     
    float4 SampleDetailShapeNoise(in float3 location, in float scale)
    {
        return params.detailShapeNoise.Sample(params.detailShapeSampler, location * scale);
    }
     
    float3 SampleWeather(in float3 location)
    {
        return params.weather.SampleLevel(params.weatherSampler, location.xy * params.weatherScale + 0.5f, 0.f);
    }

    float3 SampleCurlNoise(in float3 location)
    {
        return params.curlNoise.SampleLevel(params.curlNoiseSampler, location.xy * params.curlNoiseScale, 0.f);
    }

    float3 ComputeAdjustedLocation(in float3 location)
    {
        float3 offset = float3((SampleCurlNoise(location) * 2.f - 1.f) * params.curlMaxOffset);
        offset += float3((SampleCurlNoise(location.zyx) * 2.f - 1.f) * params.curlMaxOffset);

        return location + offset;
    }

    float ComputeDetailNoise(in float3 location, in float scale, in float h,  in float densityAltitude)
    {
        float4 noise = SampleDetailShapeNoise(location, scale);

        const float highFrequencyFBM = noise.r * 0.625f + noise.g * 0.25f + noise.z * 0.125f;

        return lerp(highFrequencyFBM, 1.f - highFrequencyFBM, saturate(h * 10.f));
    }

    float2 SampleDensity(in float3 location, in int detailLevel)
    {
        const float totalH = ComputeHeightAlphaAtLocation(location);

        float3 windDir = float3(1.f, 1.f, 0.f);
        location += totalH * windDir * 500.f;

        location = ComputeAdjustedLocation(location);

        const float3 weatherMap = SampleWeather(location);

        float cloudsCoverage = saturate(weatherMap.b + params.globalCoverageOffset);
        float cloudsHeight = saturate(weatherMap.g + params.globalCloudsHeightOffset);

        if(cloudsCoverage < 0.001f || cloudsHeight < 0.001f)
        {
            return 0.f;
        }

        const float h = totalH / max(cloudsHeight, 0.0001f);

        float densityAltitude =  params.densityLUT.SampleLevel(params.densityLUTSampler, float2(cloudsHeight, 1.f - totalH), 0.f);

        const float baseCloudNoise = SampleBaseShapeNoise(location);

        float baseCloud = saturate(baseCloudNoise - (1.f - densityAltitude * cloudsCoverage));
        baseCloud *= max(cloudsCoverage, 0.f);

        if(baseCloud <= 0.001f)
        {
            return 0.f;
        }
        
#if 0
        location += frac(params.time * windDir * 100.f * params.detailShapeScale) / params.detailShapeScale;
#endif

        float cloudDensity = baseCloud;

        const float edgeDetail = 1.f;

        {
            const float noiseComposite = ComputeDetailNoise(location, params.detailShapeNoiseScale0, h, densityAltitude);
            cloudDensity = Remap<float>(cloudDensity, min(params.detailShapeNoiseStrength0 * edgeDetail * noiseComposite, 0.999f), 1.f, 0.f, 1.f);
        }

        {
            const float noiseComposite = ComputeDetailNoise(location, params.detailShapeNoiseScale1, h, densityAltitude);
            cloudDensity = Remap<float>(cloudDensity, min(params.detailShapeNoiseStrength1 * edgeDetail * noiseComposite, 0.999f), 1.f, 0.f, 1.f);
        }

        cloudDensity = pow(saturate(cloudDensity), Remap<float>(totalH, 0.05f, 0.4f, 1.f, 0.45f));

        float ambient = max(pow(1.f - saturate(cloudDensity), 0.25f) * saturate(h), 0.4f);

        const float finalDensity = cloudDensity * params.globalDensity;
        
        return float2(finalDensity, ambient);
    }
};


#ifdef DS_CloudscapeDS
CloudsSampler CreateCloudscapeSampler()
{
    CloudsSamplerParams csParams;
    csParams.baseShapeNoise            = u_baseShapeNoise;
    csParams.baseShapeSampler          = u_linearRepeatSampler;
    csParams.detailShapeNoise          = u_detailShapeNoise;
    csParams.detailShapeSampler        = u_linearRepeatSampler;
    csParams.weather                   = u_weatherMap;
    csParams.weatherSampler            = u_linearRepeatSampler;
    csParams.curlNoise                 = u_curlNoise;
    csParams.curlNoiseSampler          = u_linearRepeatSampler;
    csParams.densityLUT                = u_densityLUT;
    csParams.densityLUTSampler         = u_linearClampSampler;
    csParams.baseShapeScale            = u_cloudscapeConstants.baseShapeNoiseScale;
    csParams.detailShapeNoiseStrength0 = u_cloudscapeConstants.detailShapeNoiseStrength0;
    csParams.detailShapeNoiseScale0    = u_cloudscapeConstants.detailShapeNoiseScale0;
    csParams.detailShapeNoiseStrength1 = u_cloudscapeConstants.detailShapeNoiseStrength1;
    csParams.detailShapeNoiseScale1    = u_cloudscapeConstants.detailShapeNoiseScale1;
    csParams.curlNoiseScale            = u_cloudscapeConstants.curlNoiseScale;
	csParams.curlMaxOffset             = u_cloudscapeConstants.curlMaxoffset;
    csParams.weatherScale              = u_cloudscapeConstants.weatherMapScale;
    csParams.cloudsMinHeight           = u_cloudscapeConstants.cloudscapeInnerHeight;
    csParams.cloudsMaxHeight           = u_cloudscapeConstants.cloudscapeOuterHeight;
    csParams.globalDensity             = u_cloudscapeConstants.globalDensity;
    csParams.globalCoverageOffset      = u_cloudscapeConstants.globalCoverageOffset;
    csParams.globalCloudsHeightOffset  = u_cloudscapeConstants.globalCloudsHeightOffset;
    csParams.atmosphereCenter          = u_cloudscapeConstants.cloudsAtmosphereCenter;
    csParams.time                      = u_cloudscapeConstants.time;
    CloudsSampler cs                   = CloudsSampler::Create(csParams);

    return cs;
}
#endif

#endif // CLOUD_SAMPLER_HLSLI