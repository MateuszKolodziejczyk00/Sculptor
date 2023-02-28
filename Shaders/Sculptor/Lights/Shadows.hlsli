
#define PCSS_SHADOW_SAMPLES_NUM 24


static const float2 pcssShadowSamples[PCSS_SHADOW_SAMPLES_NUM] =
{
    float2(0.f, 0.f),
    float2(-0.080782, 0.604663),
    float2(0.393353, -0.362956),
    float2(0.964293, 0.982422),
    float2(-0.742702, -0.803949),
    float2(-0.904988, 0.934018),
    float2(-0.994019, 0.077852),
    float2(0.925961, -0.958982),
    float2(0.980102, 0.134069),
    float2(-0.293495, 0.270247),
    float2(0.218238, -0.980224),
    float2(0.536546, 0.548205),
    float2(0.364359, 0.080905),
    float2(-0.460982, 0.249306),
    float2(-0.853267, -0.329570),
    float2(0.849603, -0.507252),
    float2(-0.277445, -0.944334),
    float2(0.229224, 0.960632),
    float2(0.009186, -0.607045),
    float2(-0.360952, 0.952514),
    float2(-0.926329, 0.509752),
    float2(0.534226, -0.686352),
    float2(0.534226, -0.686352),
    float2(-0.551160, 0.599598)};


#define PCF_SHADOW_SAMPLES_NUM 13


static const float2 pcfShadowSamples[PCF_SHADOW_SAMPLES_NUM] = {
    float2(0.f, 0.f),
    float2(-0.847682, -0.289223),
    float2(0.996032, 0.655202),
    float2(-0.292642, 0.906186),
    float2(-0.996887, -0.578296),
    float2(0.936888, -0.959595),
    float2(-0.816645, 0.355573),
    float2(0.937804, -0.124607),
    float2(-0.279643, -0.970275),
    float2(0.290383, 0.855342),
    float2(-0.495529, -0.311257),
    float2(-0.997315, 0.961975),
    float2(-0.062288, 0.383587)
};



#define SM_QUALITY_LOW 0
#define SM_QUALITY_MEDIUM 1
#define SM_QUALITY_HIGH 2


uint GetShadowMapFaceIndex(float3 lightToSurface)
{
	const float3 lightToSurfaceAbs = abs(lightToSurface);
	uint faceIndex = 0;
	if(lightToSurfaceAbs.x >= lightToSurfaceAbs.y && lightToSurfaceAbs.x >= lightToSurfaceAbs.z)
	{
        // We cannot use ternary operator here, because it produces DXC compiler internal error
		//faceIndex = lightToSurface.x > 0.f ? 0 : 1;
		if(lightToSurface.x > 0.f)
        {
            faceIndex = 0;
        }
        else
        {
            faceIndex = 1;
        }
    }
	else if(lightToSurfaceAbs.y >= lightToSurfaceAbs.z)
	{
		//faceIndex = lightToSurface.y > 0.f ? 2 : 3;
		if(lightToSurface.y > 0.f)
        {
            faceIndex = 2;
        }
        else
        {
            faceIndex = 3;
        }
	}
	else
	{
		//faceIndex = lightToSurface.z > 0.f ? 4 : 5;
		if(lightToSurface.z > 0.f)
        {
            faceIndex = 4;
        }
        else
        {
            faceIndex = 5;
        }
	}
	return faceIndex;
}


uint GetShadowMapQuality(uint shadowMapIdx)
{
    if(shadowMapIdx < u_shadowsSettings.highQualitySMEndIdx)
    {
        return SM_QUALITY_HIGH;
    }
    else if(shadowMapIdx < u_shadowsSettings.mediumQualitySMEndIdx)
    {
        return SM_QUALITY_MEDIUM;
    }

    return SM_QUALITY_LOW;
}


float2 GetShadowMapPixelSize(uint quality)
{
    if(quality == SM_QUALITY_HIGH)
    {
        return u_shadowsSettings.highQualitySMPixelSize;
    }
    else if(quality == SM_QUALITY_MEDIUM)
    {
        return u_shadowsSettings.mediumQualitySMPixelSize;
    }

    return u_shadowsSettings.lowQualitySMPixelSize;
}


void GetPointShadowMapViewAxis(uint faceIdx, out float3 rightVector, out float3 upVector)
{
    const float3 rightVectors[6] =
    {
        float3(0.f, 1.f, 0.f),
        float3(0.f, -1.f, 0.f),
        float3(-1.f, 0.f, 0.f),
        float3(1.f, 0.f, 0.f),
        float3(0.f, 1.f, 0.f),
        float3(0.f, 1.f, 0.f),
    };
    
    const float3 upVectors[6] =
    {
        float3(0.f, 0.f, 1.f),
        float3(0.f, 0.f, 1.f),
        float3(0.f, 0.f, 1.f),
        float3(0.f, 0.f, 1.f),
        float3(-1.f, 0.f, 0.f),
        float3(1.f, 0.f, 0.f),
    };

    rightVector = rightVectors[faceIdx];
    upVector    = upVectors[faceIdx];
}


void ComputeShadowProjectionParams(float near, float far, out float p20, out float p23)
{
    p20 = -near / (far - near);
    p23 = -far * p20;
}


float ComputeShadowLinearDepth(float ndcDepth, float p20, float p23)
{
    return p23 / (ndcDepth - p20);
}


float ComputeShadowNDCDepth(float linearDepth, float p20, float p23)
{
    return (p20 * linearDepth + p23) / linearDepth;
}


float ComputeBias(float3 surfaceNormal, float3 surfaceToLight)
{
    const float biasFactor = 0.012f;
    return min(biasFactor * tan(acos(dot(surfaceNormal, surfaceToLight))) + 0.0017f, 0.15f);
}


float2 ComputeKernelScale(float3 surfaceNormal, float3 shadowMapRightWS, float3 shadowMapUpWS)
{
    const float minKernelScale = 0.6f;
    return 1.f - (1.f - minKernelScale) * float2(dot(surfaceNormal, shadowMapRightWS), dot(surfaceNormal, shadowMapUpWS));
}


float FindAverageOccluderDepth(Texture2D shadowMap, SamplerState occludersSampler, float2 shadowMapPixelSize, float2 shadowMapUV, float ndcDepth, float p20, float p23)
{
    float occludersLinearDepth = 0.f;
    float occludersNum = 0.f;

    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            const float2 sampleUV = shadowMapUV + float2(i, j) * shadowMapPixelSize * 3.f;
            const float shadowMapDepth = shadowMap.Sample(occludersSampler, sampleUV).x;

            if(shadowMapDepth > ndcDepth)
            {
                occludersLinearDepth += ComputeShadowLinearDepth(shadowMapDepth, p20, p23);
                occludersNum += 1.f;
            }
        }
    }

    return occludersNum > 0.f ? occludersLinearDepth / occludersNum : -1.f;
}


float2 ComputePenumbraSize(float surfaceDepth, float occluderDepth, float lightRadius, float nearPlane, float2 shadowMapPixelSize)
{
    const float minPenumbraPixelsNum = 2.f;
    const float maxPenumbraPixelsNum = 4.f;
    
    if(occluderDepth > 0.f)
    {
        const float penumbraSize = (((surfaceDepth - occluderDepth) * lightRadius) / occluderDepth);
        const float penumbraNear = (nearPlane / surfaceDepth) * penumbraSize;
        return clamp(2.f * penumbraNear, minPenumbraPixelsNum * shadowMapPixelSize, maxPenumbraPixelsNum * shadowMapPixelSize);
    }
    else
    {
        return minPenumbraPixelsNum * shadowMapPixelSize;
    }
}


float EvaluateShadowsPCSS(Texture2D shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapUV, float ndcDepth, float2 penumbraSize, float noise)
{
    float shadow = 0.f;

    const float2x2 samplesRotation = NoiseRotation(noise);

    [[unroll]]
    for (uint i = 0; i < PCSS_SHADOW_SAMPLES_NUM; ++i)
    {
        float2 offset = pcssShadowSamples[i] * penumbraSize;
        offset = mul(samplesRotation, offset);
        const float2 uv = shadowMapUV + offset;
        shadow += shadowMap.SampleCmp(shadowSampler, uv, ndcDepth);
    }

    shadow /= PCSS_SHADOW_SAMPLES_NUM;

    return shadow;
}


float EvaluateShadowsPCF(Texture2D shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapUV, float ndcDepth, float2 penumbraSize, float noise)
{
    float shadow = 0.f;

    const float2x2 samplesRotation = NoiseRotation(noise);

    [[unroll]]
    for (uint i = 0; i < PCF_SHADOW_SAMPLES_NUM; ++i)
    {
        float2 offset = pcfShadowSamples[i] * penumbraSize;
        offset = mul(samplesRotation, offset);
        const float2 uv = shadowMapUV + offset;
        shadow += shadowMap.SampleCmp(shadowSampler, uv, ndcDepth);
    }

    shadow /= PCF_SHADOW_SAMPLES_NUM;
    
    return shadow;
}


float EvaluatePointLightShadows(ShadedSurface surface, float3 pointLightLocation, float pointLightAttenuationRadius, uint shadowMapFirstFaceIdx)
{
    const uint shadowMapFaceIdx = GetShadowMapFaceIndex(surface.location - pointLightLocation);
    const uint shadowMapIdx = shadowMapFirstFaceIdx + shadowMapFaceIdx;

    float3 shadowMapRight;
    float3 shadowMapUp;
    GetPointShadowMapViewAxis(shadowMapFaceIdx, shadowMapRight, shadowMapUp);

    const float2 kernelScale = ComputeKernelScale(surface.geometryNormal, shadowMapRight, shadowMapUp);

    const float4 surfaceShadowCS = mul(u_shadowMapViews[shadowMapIdx].viewProjectionMatrix, float4(surface.location, 1.f));
    const float3 surfaceShadowNDC = surfaceShadowCS.xyz / surfaceShadowCS.w;

    const float2 shadowMapUV = surfaceShadowNDC.xy * 0.5f + 0.5f;

    const float nearPlane = u_shadowsSettings.shadowViewsNearPlane;
    const float farPlane = pointLightAttenuationRadius;
    float p20, p23;
    ComputeShadowProjectionParams(nearPlane, farPlane, p20, p23);

    const uint shadowMapQuality = GetShadowMapQuality(shadowMapIdx);

    const float bias = ComputeBias(surface.geometryNormal, normalize(pointLightLocation - surface.location));
    const float ndcMinBias = shadowMapQuality == SM_QUALITY_LOW ? 0.00034f : 0.00024f;
    const float surfaceLinearDepth = ComputeShadowLinearDepth(surfaceShadowNDC.z, p20, p23);
    float surfaceNDCDepth = ComputeShadowNDCDepth(surfaceLinearDepth - bias, p20, p23);
    surfaceNDCDepth = max(surfaceShadowNDC.z + ndcMinBias, surfaceNDCDepth);
    
    const float2 shadowMapPixelSize = GetShadowMapPixelSize(shadowMapQuality);

    if(shadowMapQuality == SM_QUALITY_LOW)
    {
        // 1 tap shadow
        return u_shadowMaps[shadowMapIdx].SampleCmp(u_shadowSampler, shadowMapUV, surfaceNDCDepth);
    }
    else if(shadowMapQuality == SM_QUALITY_MEDIUM)
    {
        // PCF shadows
        const float noise = InterleavedGradientNoise(float2(surface.location.x - surface.location.z, surface.location.y + surface.location.z));
        
        const float2 penumbraSize = 1.f * shadowMapPixelSize;
        return EvaluateShadowsPCF(u_shadowMaps[shadowMapIdx], u_shadowSampler, shadowMapUV, surfaceNDCDepth, penumbraSize, noise);
    }
    else // if(shadowMapQuality == SM_QUALITY_HIGH)
    {
        // PCSS shadows
        const float noise = Random(float2(surface.location.x - surface.location.z, surface.location.y + surface.location.z));

        const float avgOccluderDepth = FindAverageOccluderDepth(u_shadowMaps[shadowMapIdx], u_occludersSampler, shadowMapPixelSize, shadowMapUV, surfaceNDCDepth, p20, p23);

        const float lightArea = 0.15f;
        const float2 penumbraSize = ComputePenumbraSize(surfaceLinearDepth, avgOccluderDepth, lightArea, nearPlane, shadowMapPixelSize) * kernelScale;

        const float shadow = EvaluateShadowsPCSS(u_shadowMaps[shadowMapIdx], u_shadowSampler, shadowMapUV, surfaceNDCDepth, penumbraSize, noise);
    
        return shadow;
    }
}