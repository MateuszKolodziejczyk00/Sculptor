#ifndef CLOUD_SAMPLER_HLSLI
#define CLOUD_SAMPLER_HLSLI

#include "Utils/Sampling.hlsli"

struct CloudsSamplerParams
{
    Texture3D<float4> baseShapeNoise;
    SamplerState      baseShapeSampler;

    Texture3D<float4> detailShapeNoise;
    SamplerState      detailShapeSampler;

    Texture2D<float4> weather;
    SamplerState      weatherSampler;

    Texture3D<float3> curlNoise;
    SamplerState      curlNoiseSampler;

    Texture2D<float> densityLUT;
    SamplerState     densityLUTSampler;

    Texture2D<float2> cirrusMask;

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

    float globalCoverageMultiplier;
    float globalCloudsHeightMultiplier;

    float3 atmosphereCenter;

    float time;
};

#define CLOUDS_DETAIL_EROSION 1
#define CLOUDS_DETAIL_CURL    2

#define CLOUDS_HIGHEST_DETAIL_LEVEL 2

#define CLOUDS_DETAIL_PRESET_PROBE         1
#define CLOUDS_DETAIL_PRESET_TRANSMITTANCE 1
#define CLOUDS_DETAIL_PRESET_MAIN_VIEW     CLOUDS_HIGHEST_DETAIL_LEVEL

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

		return noise.x;
    }

	float3 SampleCirrusDensity(in float3 location)
	{
		const float2 uv = location.xy / 50000.f + 0.5f;

		float mask = params.cirrusMask.Sample(params.baseShapeSampler, uv).x;

		return float3(saturate(mask - 0.01f) * 0.3f, 1.f, 1.f);
	}

	float ComputeDensityAltitude(in float cloudsType, in float totalH)
	{
		return Remap<float>(totalH, 0.f, 0.2f, 0.f, 1.f) * Remap<float>(totalH, 0.9f * cloudsType, 1.1f * cloudsType, 1.f, 0.f);
	}

    float4 SampleDetailShapeNoise(in float3 location, in float scale)
    {
        return params.detailShapeNoise.Sample(params.detailShapeSampler, location * scale);
    }
     
    float4 SampleWeather(in float3 location)
    {
		return SampleTricubicBSpline(params.weather, params.weatherSampler, location.xy * params.weatherScale + 0.5f + 1.f, 2048u, 1.f / 2048.f);
    }

    float3 SampleCurlNoise(in float3 location)
    {
        return params.curlNoise.SampleLevel(params.curlNoiseSampler, location * params.curlNoiseScale, 0.f) * 2.f - 1.f;
    }

    float3 ComputeAdjustedLocation(in float3 location, in float totalH)
    {
		const float3 offset = SampleCurlNoise(location) * params.curlMaxOffset;

        return location + offset * (0.08f + 0.92f * Pow2(saturate(1.f - totalH * 2.f)));
    }

    float ComputeDetailNoise(in float3 location, in float scale, in float h,  in float densityAltitude)
    {
		const float highFrequencyFBM = SampleDetailShapeNoise(location, scale).x;
		return lerp(highFrequencyFBM, 1.f - highFrequencyFBM, saturate(h * 10.f));
    }

    float2 SampleDensityBase(in float3 location, in int detailLevel, in float totalH, in float4 weatherMap)
    {
        float cloudsCoverage = weatherMap.r * 0.9f;
        float cloudsType     = weatherMap.g;
		float bottomType     = 1.f - weatherMap.b;

		if (cloudsType < 0.001f)
		{
			return 0.f;
		}

        if(cloudsCoverage < 0.001f)
        {
            return 0.f;
        }

        const float h = saturate(totalH / max(cloudsType, 0.0001f));

        float densityAltitude = params.densityLUT.SampleLevel(params.densityLUTSampler, float2(cloudsType, 1.f - totalH), 0.f);

        float baseCloudNoise = SampleBaseShapeNoise(location);

		float baseCloud = Remap<float>(baseCloudNoise, 0.0f, 1.f, 0.f, 1.f);
		baseCloud *= densityAltitude;
		baseCloud = Remap<float>(baseCloud, 1.f - cloudsCoverage, 1.f, 0.f, 1.f);;
		baseCloud *= cloudsCoverage;

		baseCloud *= Remap<float>(totalH, 0.15f * bottomType, 0.35f * bottomType + 0.001f, 0.1f, 1.f);

		float lowLodDensity = baseCloud;

		float detailNoise = ComputeDetailNoise(location, params.detailShapeNoiseScale0, totalH, densityAltitude);

		baseCloud = Remap<float>(baseCloud, detailNoise * 0.3f, 1.f, 0.f, 1.f);

        if(baseCloud <= 0.001f)
        {
            return 0.f;
        }

        float cloudDensity = baseCloud;

        const float densityExponent = Remap<float>(cloudsType, 0.1f, 0.3f, 1.f, Remap<float>(totalH, 0.07f, 0.25f, lerp(2.f, 4.f, bottomType), 1.f));

        cloudDensity = pow(saturate(cloudDensity), densityExponent);
		lowLodDensity = pow(saturate(lowLodDensity), densityExponent);
        cloudDensity *= 0.5f + saturate(2.f * max(totalH - 0.3f, 0.f));
        float ambient = saturate(1.f - 0.6f * saturate(3.f * (cloudsType - totalH))) * 1.5f;

		cloudDensity = pow(saturate(cloudDensity), Remap<float>(totalH, 0.f, 0.4f, 1.f, 0.6f));

        const float finalDensity = cloudDensity * params.globalDensity;

		const float depthProbability = 0.05f + pow(lowLodDensity, Remap<float>(h, 0.3f, 0.85f, 0.5f, 2.f));
		const float verticalProbability = pow(Remap<float>(h, 0.07f, 0.14f, 0.1f, 1.f), 0.8f);

        return float2(finalDensity, ambient);
    }

    float2 SampleDensity(in float3 location, in int detailLevel)
    {
        float totalH = ComputeHeightAlphaAtLocation(location);

        float3 windDir = float3(1.f, 1.f, 0.f);

        if(detailLevel >= CLOUDS_DETAIL_CURL)
        {
            location = ComputeAdjustedLocation(location, totalH);
        }

        const float4 weatherMap = SampleWeather(location);

		const float2 base =  SampleDensityBase(location, detailLevel, totalH, weatherMap);

		return base;
    }
};


#ifdef DS_CloudscapeDS
CloudsSampler CreateCloudscapeSampler()
{
    CloudsSamplerParams csParams;
    csParams.baseShapeNoise               = u_baseShapeNoise;
    csParams.baseShapeSampler             = u_cloadsLinearRepeatSampler;
    csParams.detailShapeNoise             = u_detailShapeNoise;
    csParams.detailShapeSampler           = u_cloadsLinearRepeatSampler;
    csParams.weather                      = u_weatherMap;
    csParams.weatherSampler               = u_cloadsLinearRepeatSampler;
    csParams.curlNoise                    = u_curlNoise;
    csParams.curlNoiseSampler             = u_cloadsLinearRepeatSampler;
    csParams.densityLUT                   = u_densityLUT;
    csParams.densityLUTSampler            = u_cloadsLinearClampSampler;
	csParams.cirrusMask                   = u_cirrusCloudsMask;
    csParams.baseShapeScale               = u_cloudscapeConstants.baseShapeNoiseScale;
    csParams.detailShapeNoiseStrength0    = u_cloudscapeConstants.detailShapeNoiseStrength0;
    csParams.detailShapeNoiseScale0       = u_cloudscapeConstants.detailShapeNoiseScale0;
    csParams.detailShapeNoiseStrength1    = u_cloudscapeConstants.detailShapeNoiseStrength1;
    csParams.detailShapeNoiseScale1       = u_cloudscapeConstants.detailShapeNoiseScale1;
    csParams.curlNoiseScale               = u_cloudscapeConstants.curlNoiseScale;
    csParams.curlMaxOffset                = u_cloudscapeConstants.curlMaxoffset;
    csParams.weatherScale                 = u_cloudscapeConstants.weatherMapScale;
    csParams.cloudsMinHeight              = u_cloudscapeConstants.cloudscapeInnerHeight;
    csParams.cloudsMaxHeight              = u_cloudscapeConstants.cloudscapeOuterHeight;
    csParams.globalDensity                = u_cloudscapeConstants.globalDensity;
    csParams.globalCoverageOffset         = u_cloudscapeConstants.globalCoverageOffset;
    csParams.globalCloudsHeightOffset     = u_cloudscapeConstants.globalCloudsHeightOffset;
    csParams.globalCoverageMultiplier     = u_cloudscapeConstants.globalCoverageMultiplier;
    csParams.globalCloudsHeightMultiplier = u_cloudscapeConstants.globalCloudsHeightMultiplier;
    csParams.atmosphereCenter             = u_cloudscapeConstants.cloudsAtmosphereCenter;
    csParams.time                         = u_cloudscapeConstants.time;
    CloudsSampler cs                      = CloudsSampler::Create(csParams);

    return cs;
}
#endif

#endif // CLOUD_SAMPLER_HLSLI
