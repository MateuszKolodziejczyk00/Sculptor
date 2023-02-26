
#define PCSS_SHADOW_SAMPLES_NUM 24


static const float2 pcssShadowSamples[PCSS_SHADOW_SAMPLES_NUM] =
{
    float2(0.957701, 0.866634),
    float2(0.482099, -0.993775),
    float2(-0.315592, 0.184849),
    float2(-0.56096, -0.925292),
    float2(-0.988587, 0.936888),
    float2(0.98761, 0.0751681),
    float2(0.0485548, 0.970642),
    float2(-0.977234, 0.765648),
    float2(0.308878, 0.746603),
    float2(0.276101, 0.438703),
    float2(-0.239478, 0.59621),
    float2(0.913267, 0.51326),
    float2(-0.902157, 0.370404),
    float2(-0.059846, -0.99115),
    float2(-0.407574, 0.64159),
    float2(0.527512, 0.97528),
    float2(0.93701, -0.987079),
    float2(0.691214, 0.522812),
    float2(-0.995178, -0.804682),
    float2(0.151342, -0.68749),
    float2(-0.681326, -0.588),
    float2(0.606494, -0.587366),
    float2(-0.733634, 0.660268),
    float2(0.0101013, 0.993012)
};


#define PCF_SHADOW_SAMPLES_NUM 13


static const float2 pcfShadowSamples[PCF_SHADOW_SAMPLES_NUM] = {
    float2(0.f, 0.f),
    float2(0.576159, 0.355388),
    float2(0.998016, 0.827601),
    float2(0.353679, 0.953093),
    float2(0.00155644, 0.210852),
    float2(0.968444, 0.0202033),
    float2(0.0916776, 0.677786),
    float2(0.968902, 0.437696),
    float2(0.360179, 0.0148625),
    float2(0.645192, 0.927671),
    float2(0.252235, 0.344371),
    float2(0.00134281, 0.980987),
    float2(0.468856, 0.691794)
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
        float3(0.f, -1.f, 0.f),
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

    const float bias = ComputeBias(surface.geometryNormal, normalize(pointLightLocation - surface.location));
    const float ndcMinBias = 0.00024f;
    const float surfaceLinearDepth = ComputeShadowLinearDepth(surfaceShadowNDC.z, p20, p23);
    float surfaceNDCDepth = ComputeShadowNDCDepth(surfaceLinearDepth - bias, p20, p23);
    surfaceNDCDepth = max(surfaceShadowNDC.z + ndcMinBias, surfaceNDCDepth);

    const uint shadowMapQuality = GetShadowMapQuality(shadowMapIdx);
    
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
        const float noise = InterleavedGradientNoise(float2(surface.location.x - surface.location.z, surface.location.y + surface.location.z));

        const float avgOccluderDepth = FindAverageOccluderDepth(u_shadowMaps[shadowMapIdx], u_occludersSampler, shadowMapPixelSize, shadowMapUV, surfaceNDCDepth, p20, p23);

        const float lightArea = 0.15f;
        const float2 penumbraSize = ComputePenumbraSize(surfaceLinearDepth, avgOccluderDepth, lightArea, nearPlane, shadowMapPixelSize) * kernelScale;

        const float shadow = EvaluateShadowsPCSS(u_shadowMaps[shadowMapIdx], u_shadowSampler, shadowMapUV, surfaceNDCDepth, penumbraSize, noise);
    
        return shadow;
    }
}