
uint GetShadowMapFaceIndex(float3 lightToSurface)
{
	const float3 lightToSurfaceAbs = abs(lightToSurface);
	uint faceIndex = 0;
	if(lightToSurfaceAbs.x >= lightToSurfaceAbs.y && lightToSurfaceAbs.x >= lightToSurfaceAbs.z)
	{
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


float EvaluatePointLightShadows(float3 surfaceLocation, float3 pointLightLocation, uint shadowMapFirstFaceIdx)
{
    const uint shadowMapIdx = shadowMapFirstFaceIdx + GetShadowMapFaceIndex(surfaceLocation - pointLightLocation);

    const float4 surfaceShadowCS = mul(u_shadowMapViews[shadowMapIdx].viewProjectionMatrix, float4(surfaceLocation, 1.f));
    const float3 surfaceShadowNDC = surfaceShadowCS.xyz / surfaceShadowCS.w;

    const float2 shadowMapUV = surfaceShadowNDC.xy * 0.5f + 0.5f;

    const float bias = 0.001f;

    const float shadowValue = 1.f - u_shadowMaps[shadowMapIdx].SampleCmp(u_shadowSampler, shadowMapUV, surfaceShadowNDC.z + bias);

    return shadowValue;
}