

static const float minBias = 0.01f;
static const float maxBias = 0.05f;


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
    // Don't need to saturate dot here, if it's lower than 0 shadows are not evaluated at all
    return lerp(maxBias, minBias, dot(surfaceNormal, surfaceToLight));
}


float EvaluatePointLightShadows(float3 surfaceLocation, float3 surfaceNormal, float3 pointLightLocation, float pointLightRadius, uint shadowMapFirstFaceIdx)
{
    const uint shadowMapIdx = shadowMapFirstFaceIdx + GetShadowMapFaceIndex(surfaceLocation - pointLightLocation);

    const float4 surfaceShadowCS = mul(u_shadowMapViews[shadowMapIdx].viewProjectionMatrix, float4(surfaceLocation, 1.f));
    const float3 surfaceShadowNDC = surfaceShadowCS.xyz / surfaceShadowCS.w;

    const float2 shadowMapUV = surfaceShadowNDC.xy * 0.5f + 0.5f;

    const float farPlane = pointLightRadius;
    float p20, p23;
    ComputeShadowProjectionParams(u_shadowsSettings.shadowViewsNearPlane, farPlane, p20, p23);

    const float bias = ComputeBias(surfaceNormal, normalize(pointLightLocation - surfaceLocation));

    const float linearDepth = ComputeShadowLinearDepth(surfaceShadowNDC.z, p20, p23);
    const float ndcDepth = ComputeShadowNDCDepth(linearDepth - bias, p20, p23);

    const float shadowValue = u_shadowMaps[shadowMapIdx].SampleCmp(u_shadowSampler, shadowMapUV, ndcDepth);

    return shadowValue;
}