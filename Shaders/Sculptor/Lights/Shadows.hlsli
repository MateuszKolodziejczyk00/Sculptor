#ifndef SHADOWS_HLSLI
#define SHADOWS_HLSLI

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Random.hlsli"

#define PCSS_SHADOW_SAMPLES_NUM 24


#define SPT_SHADOWS_TECHNIQUE_NONE   0
#define SPT_SHADOWS_TECHNIQUE_DPCF   1
#define SPT_SHADOWS_TECHNIQUE_MSM    2
#define SPT_SHADOWS_TECHNIQUE_VSM    3


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


#define PCF_SHADOW_SAMPLES_NUM 9 


static const float2 pcfShadowSamples[PCF_SHADOW_SAMPLES_NUM] = {
	float2(-0.474, 0.054),
	float2(0.650, -0.644),
	float2(0.136, 0.580),
	float2(0.914, 0.802),
	float2(-0.753, -0.732),
	float2(-0.216, -0.448),
	float2(0.368, -0.864),
	float2(-0.522, 0.742),
	float2(0.484, 0.006)
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


float ComputeBias(float3 surfaceNormal, float3 surfaceToLight)
{
	const float biasFactor = 0.008f;
	return min(biasFactor * tan(acos(dot(surfaceNormal, surfaceToLight))) + 0.001f, 0.15f);
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
	const float maxPenumbraPixelsNum = 6.f;
	
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

	[unroll]
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


/** Based on "Shadows of Cold War" by Kevin Meyers, Treyarch */ 
float EvaluateShadowsDPCF(Texture2D shadowMap, SamplerState shadowSampler, float2 shadowMapUV, float linearDepth, float2 penumbraSize, float p20, float p23, float noise)
{
	const float2x2 samplesRotation = NoiseRotation(noise);

	float occluders = 0.f;
	float occludersDistSum = 0.f;
	
	for (uint i = 0; i < PCF_SHADOW_SAMPLES_NUM; ++i)
	{
		float2 offset = pcfShadowSamples[i] * penumbraSize;
		offset = mul(samplesRotation, offset);
		const float2 uv = shadowMapUV + offset;
		
		const float4 depths = shadowMap.Gather(shadowSampler, uv);

		[unroll]
		for (uint depthIdx = 0; depthIdx < 4; ++depthIdx)
		{
			const float occluderLinearDepth = ComputeShadowLinearDepth(depths[depthIdx], p20, p23);
			const float distDiff = -occluderLinearDepth + linearDepth;
			const float occluder = step(0.f, distDiff);

			occluders += occluder;
			occludersDistSum += occluderLinearDepth * occluder;
		}
	}

	const float occluderAvgDist = occludersDistSum * rcp(occluders);
	const float pcfWeight = saturate(occluderAvgDist * 0.25f);

	float percentageOccluded = occluders * rcp(PCF_SHADOW_SAMPLES_NUM * 4.f);

	percentageOccluded = 2.f * percentageOccluded - 1.f;
	float occludedSign = sign(percentageOccluded);
	percentageOccluded = 1.f - occludedSign * percentageOccluded;
	
	percentageOccluded = lerp(Pow3(percentageOccluded), percentageOccluded, pcfWeight);
	percentageOccluded = 1.f - percentageOccluded;
	percentageOccluded *= occludedSign;
	percentageOccluded = 0.5f * percentageOccluded + 0.5f;

	return 1.f - percentageOccluded;
}

// Based on MJP implementation: https://github.com/TheRealMJP/Shadows/blob/master/Shadows/MSM.hlsl
float4 ConvertOptimizedMoments(in float4 optimizedMoments)
{
	optimizedMoments[0] -= 0.035955884801f;
	return mul(optimizedMoments, float4x4(0.2227744146f, 0.1549679261f, 0.1451988946f, 0.163127443f,
										  0.0771972861f, 0.1394629426f, 0.2120202157f, 0.2591432266f,
										  0.7926986636f, 0.7963415838f, 0.7258694464f, 0.6539092497f,
										  0.0319417555f,-0.1722823173f,-0.2758014811f,-0.3376131734f));
}

// Based on MJP implementation: https://github.com/TheRealMJP/Shadows/blob/master/Shadows/MSM.hlsl
float ComputeMSMHamburger(in float4 moments, in float fragmentDepth , in float depthBias, in float momentBias)
{
	// Bias input data to avoid artifacts
	float4 b = lerp(moments, float4(0.5f, 0.5f, 0.5f, 0.5f), momentBias);
	float3 z;
	z[0] = fragmentDepth - depthBias;

	// Compute a Cholesky factorization of the Hankel matrix B storing only non-
	// trivial entries or related products
	float L32D22 = mad(-b[0], b[1], b[2]);
	float D22 = mad(-b[0], b[0], b[1]);
	float squaredDepthVariance = mad(-b[1], b[1], b[3]);
	float D33D22 = dot(float2(squaredDepthVariance, -L32D22), float2(D22, L32D22));
	float InvD22 = 1.0f / D22;
	float L32 = L32D22 * InvD22;

	// Obtain a scaled inverse image of bz = (1,z[0],z[0]*z[0])^T
	float3 c = float3(1.0f, z[0], z[0] * z[0]);

	// Forward substitution to solve L*c1=bz
	c[1] -= b.x;
	c[2] -= b.y + L32 * c[1];

	// Scaling to solve D*c2=c1
	c[1] *= InvD22;
	c[2] *= D22 / D33D22;

	// Backward substitution to solve L^T*c3=c2
	c[1] -= L32 * c[2];
	c[0] -= dot(c.yz, b.xy);

	// Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions
	// z[1] and z[2]
	float p = c[1] / c[2];
	float q = c[0] / c[2];
	float D = (p * p * 0.25f) - q;
	float r = sqrt(D);
	z[1] =- p * 0.5f - r;
	z[2] =- p * 0.5f + r;

	// Compute the shadow intensity by summing the appropriate weights
	float4 switchVal = (z[2] < z[0]) ? float4(z[1], z[0], 1.0f, 1.0f) :
					  ((z[1] < z[0]) ? float4(z[0], z[1], 0.0f, 1.0f) :
					  float4(0.0f,0.0f,0.0f,0.0f));
	float quotient = (switchVal[0] * z[2] - b[0] * (switchVal[0] + z[2]) + b[1])/((z[2] - switchVal[1]) * (z[0] - z[1]));
	float shadowIntensity = switchVal[2] + switchVal[3] * quotient;
	return 1.0f - saturate(shadowIntensity);
}


float EvaluateShadowsMSM(Texture2D shadowMap, SamplerState shadowSampler, float2 shadowMapUV, float linearDepth, float farPlane)
{
#ifdef FRAGMENT_SHADER
	float4 moments = shadowMap.Sample(shadowSampler, shadowMapUV);
#else
	float4 moments = shadowMap.SampleLevel(shadowSampler, shadowMapUV, 0);
#endif // FRAGMENT_SHADER
	moments = ConvertOptimizedMoments(moments);
	return ComputeMSMHamburger(moments, linearDepth / farPlane, 0.f, 0.000003f);
}


float VSMReduceLightLeaking(in float visibility, in float lightLeakThreshold)
{
	return saturate((visibility - lightLeakThreshold) / (1.f - lightLeakThreshold));
}


float EvaluateShadowsAtLine(in Texture2D shadowMap, in SamplerState shadowSampler, in float2 shadowMapBeginUV, in float2 shadowMapEndUV, in float linearDepth, in uint samplesNum)
{
	const float adjustedDepth = linearDepth;

	const float2 stepSize = (shadowMapEndUV - shadowMapBeginUV) / (samplesNum - 1);

	float visibility = 0.f;
	float weightsSum = 0.f;

	for (uint i = 0; i < samplesNum; ++i)
	{
		const float2 sampleUV = shadowMapBeginUV + stepSize * i;

		const float sampleWeight = min(i + 1, samplesNum - i);

		const float shadowMapDepth = shadowMap.SampleLevel(shadowSampler, sampleUV, 0.f).x;
		visibility += step(shadowMapDepth, adjustedDepth) * sampleWeight;

		weightsSum += sampleWeight;
	}

	return visibility / weightsSum;
}


float EvaluateShadowsVSM(in Texture2D shadowMap, in SamplerState shadowSampler, in float2 shadowMapUV, in float linearDepth)
{
	const float adjustedDepth = linearDepth - 0.002f;
	const float2 moments = shadowMap.SampleLevel(shadowSampler, shadowMapUV, 0.f).xy;
	if (adjustedDepth > moments.x)
	{
		return 1.f;
	}
	const float variance = abs(moments.y - moments.x * moments.x);
	const float d = moments.x - adjustedDepth;
	return VSMReduceLightLeaking(saturate(variance / (variance + d * d)), 0.9);
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

	const float bias = 0.0004f;
	const float surfaceNDCDepth = surfaceShadowNDC.z + bias;
	const float surfaceLinearDepth = ComputeShadowLinearDepth(surfaceNDCDepth, p20, p23);

	if (u_shadowsSettings.shadowMappingTechnique == SPT_SHADOWS_TECHNIQUE_DPCF)
	{
		const float noise = Random(float2(surface.location.x - surface.location.z, surface.location.y + surface.location.z) + float2(u_gpuSceneFrameConstants.time * 0.3f, u_gpuSceneFrameConstants.time * -0.4f));
		const float2 penumbraSize = rcp(256.f);
		return EvaluateShadowsDPCF(u_shadowMaps[shadowMapIdx], u_shadowMapSampler, shadowMapUV, surfaceLinearDepth, penumbraSize, p20, p23, noise);
	}
	else if (u_shadowsSettings.shadowMappingTechnique == SPT_SHADOWS_TECHNIQUE_MSM)
	{
		return EvaluateShadowsMSM(u_shadowMaps[shadowMapIdx], u_linearShadowMapSampler, shadowMapUV, surfaceLinearDepth, farPlane);
	}

	return 1.f;
}


float EvaluatePointLightShadowsAtLocation(in float3 worldLocation, in float3 pointLightLocation, in float pointLightAttenuationRadius, in uint shadowMapFirstFaceIdx)
{
	const uint shadowMapFaceIdx = GetShadowMapFaceIndex(worldLocation - pointLightLocation);
	const uint shadowMapIdx = shadowMapFirstFaceIdx + shadowMapFaceIdx;

	float3 shadowMapRight;
	float3 shadowMapUp;
	GetPointShadowMapViewAxis(shadowMapFaceIdx, shadowMapRight, shadowMapUp);

	const float4 sampleShadowCS = mul(u_shadowMapViews[shadowMapIdx].viewProjectionMatrix, float4(worldLocation, 1.f));
	const float3 sampleShadowNDC = sampleShadowCS.xyz / sampleShadowCS.w;

	const float2 shadowMapUV = sampleShadowNDC.xy * 0.5f + 0.5f;

	if (u_shadowsSettings.shadowMappingTechnique == SPT_SHADOWS_TECHNIQUE_DPCF)
	{
		const float shadowMapDepth = u_shadowMaps[shadowMapIdx].SampleLevel(u_shadowMapSampler, shadowMapUV, 0).x;
		return step(shadowMapDepth, sampleShadowNDC.z);
	}
	else if (u_shadowsSettings.shadowMappingTechnique == SPT_SHADOWS_TECHNIQUE_MSM)
	{
		const float nearPlane = u_shadowsSettings.shadowViewsNearPlane;
		const float farPlane = pointLightAttenuationRadius;
	   
		const float surfaceNDCDepth = sampleShadowNDC.z;

		float p20, p23;
		ComputeShadowProjectionParams(nearPlane, farPlane, p20, p23);

		const float surfaceLinearDepth = ComputeShadowLinearDepth(surfaceNDCDepth, p20, p23);

		return EvaluateShadowsMSM(u_shadowMaps[shadowMapIdx], u_linearShadowMapSampler, shadowMapUV, surfaceLinearDepth, farPlane);
	}

	return 1.f;
}

#ifdef DS_ViewShadingInputDS

float EvaluateCascadedShadowsAtLine(in float3 beginWorldLocation, in float3 endWorldLocation, in uint samplesNum, in uint firstCascadeIdx, in uint cascadesNum)
{
	uint selectedCascadeIdx = IDX_NONE_32;
	float2 lineBeginUV = 0.f;
	float2 lineEndUV = 0.f;
	float sampleDepth = 0.f;

	for(uint cascadeIdx = firstCascadeIdx; cascadeIdx < firstCascadeIdx + cascadesNum; ++cascadeIdx)
	{
		const float4 cascadeShadowBeginCS = mul(u_shadowMapCascadeViews[cascadeIdx].viewProjectionMatrix, float4(beginWorldLocation, 1.f));
		if (any(cascadeShadowBeginCS.xy > cascadeShadowBeginCS.w) || any(cascadeShadowBeginCS.xy < -cascadeShadowBeginCS.w))
		{
			continue;
		}

		const float4 cascadeShadowEndCS = mul(u_shadowMapCascadeViews[cascadeIdx].viewProjectionMatrix, float4(endWorldLocation, 1.f));
		if (any(cascadeShadowEndCS.xy > cascadeShadowEndCS.w) || any(cascadeShadowEndCS.xy < -cascadeShadowEndCS.w))
		{
			continue;
		}

		const float3 cascadeShadowBeginNDC = cascadeShadowBeginCS.xyz / cascadeShadowBeginCS.w;
		lineBeginUV = cascadeShadowBeginNDC.xy * 0.5f + 0.5f;

		const float3 cascadeShadowEndNDC = cascadeShadowEndCS.xyz / cascadeShadowEndCS.w;
		lineEndUV = cascadeShadowEndNDC.xy * 0.5f + 0.5f;

		sampleDepth = lerp(cascadeShadowBeginNDC.z, cascadeShadowEndNDC.z, 0.5f);

		selectedCascadeIdx = cascadeIdx;
		break;
	}
	
	float visibility = 0.f;
	
	if(selectedCascadeIdx != IDX_NONE_32)
	{
		visibility = EvaluateShadowsAtLine(u_shadowMapCascades[selectedCascadeIdx], u_shadowMapSampler, lineBeginUV, lineEndUV, sampleDepth, samplesNum);
	}

	return visibility;
}

float EvaluateCascadedShadowsAtLocation(in float3 worldLocation, in uint firstCascadeIdx, in uint cascadesNum)
{
	for(uint cascadeIdx = firstCascadeIdx; cascadeIdx < firstCascadeIdx + cascadesNum; ++cascadeIdx)
	{
		const float4 cascadeShadowCS = mul(u_shadowMapCascadeViews[cascadeIdx].viewProjectionMatrix, float4(worldLocation, 1.f));
		if (any(cascadeShadowCS.xy > cascadeShadowCS.w) || any(cascadeShadowCS.xy < -cascadeShadowCS.w))
		{
			continue;
		}
		const float3 cascadeShadowNDC = cascadeShadowCS.xyz / cascadeShadowCS.w;
		const float2 cascadeShadowUV = cascadeShadowNDC.xy * 0.5f + 0.5f;

		return EvaluateShadowsVSM(u_shadowMapCascades[cascadeIdx], u_linearShadowMapSampler, cascadeShadowUV, cascadeShadowNDC.z);
	}

	return 0.f;
}

#endif // RENDER_VIEW_LIGHTING

#endif // SHADOWS_HLSLI
