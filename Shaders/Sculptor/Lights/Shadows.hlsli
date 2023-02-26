
#define SHADOW_SAMPLES_NUM 24


static const float2 shadowSamples[SHADOW_SAMPLES_NUM] =
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


float S_Curve(float x, float steepness)
{
    const float val = x * 2.f - 1.f;
    return saturate((atan(val * steepness) / atan(steepness)) * 0.5f + 0.5f);
}


float PostProcessShadowValue(float shadow)
{
    //return S_Curve(shadow, 1.3f);
    return shadow;
}


float2 ComputeKernelScale(float3 surfaceNormal, float3 shadowMapRightWS, float3 shadowMapUpWS)
{
    return 1.f - 0.4f * float2(dot(surfaceNormal, shadowMapRightWS), dot(surfaceNormal, shadowMapUpWS));
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
    float penumbraSize = (((surfaceDepth - occluderDepth) * lightRadius) / occluderDepth) * 0.9;
    const float penumbraNear = (nearPlane / surfaceDepth) * penumbraSize;
    return clamp(2.f * penumbraNear, 2.5f * shadowMapPixelSize, 4.f * shadowMapPixelSize);
}


float EvaluateShadowsPCSS(Texture2D shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapUV, float ndcDepth, float2 penumbraSize, float noise)
{
    float shadow = 0.f;

    const float2x2 samplesRotation = NoiseRotation(noise);

    [[unroll]]
    for (uint i = 0; i < SHADOW_SAMPLES_NUM; ++i)
    {
        float2 offset = shadowSamples[i] * penumbraSize;
        offset = mul(samplesRotation, offset);
        const float2 uv = shadowMapUV + offset;
        shadow += shadowMap.SampleCmp(shadowSampler, uv, ndcDepth);
    }

    shadow /= SHADOW_SAMPLES_NUM;

    
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

    const float surfaceLinearDepth = ComputeShadowLinearDepth(surfaceShadowNDC.z, p20, p23);
    float surfaceNDCDepth = ComputeShadowNDCDepth(surfaceLinearDepth - bias, p20, p23);
    surfaceNDCDepth = max(surfaceShadowNDC.z + 0.00024f, surfaceNDCDepth);

    const uint shadowMapQuality = GetShadowMapQuality(shadowMapIdx);
    
    const float2 shadowMapPixelSize = GetShadowMapPixelSize(shadowMapQuality);

    const float noise = InterleavedGradientNoise(float2(surface.location.x - surface.location.z, surface.location.y + surface.location.z));

    const float avgOccluderDepth = FindAverageOccluderDepth(u_shadowMaps[shadowMapIdx], u_occludersSampler, shadowMapPixelSize, shadowMapUV, surfaceNDCDepth, p20, p23);

    float2 penumbraSize;
    if(avgOccluderDepth > 0.f)
    {
        penumbraSize = ComputePenumbraSize(surfaceLinearDepth, avgOccluderDepth, 0.15f, nearPlane, shadowMapPixelSize);
    }
    else
    {
        penumbraSize = 2.5f * shadowMapPixelSize;
    }
    penumbraSize *= kernelScale;

    float shadow = EvaluateShadowsPCSS(u_shadowMaps[shadowMapIdx], u_shadowSampler, shadowMapUV, surfaceNDCDepth, penumbraSize, noise);
    shadow = PostProcessShadowValue(shadow);
    
    return shadow;
}