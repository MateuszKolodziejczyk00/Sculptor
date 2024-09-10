#ifndef SR_DENOISING_COMMON_HLSLI
#define SR_DENOISING_COMMON_HLSLI


#define MAX_ACCUMULATED_FRAMES_NUM 38.f

float ComputeSpecularNormalWeight(in float3 centerNormal, in float3 sampleNormal, in float roughness)
{
	const float power = 1.f + 64.f * (1.f - roughness);
	return pow(saturate(dot(centerNormal, sampleNormal)), power);
}


float ComputeNormalWeight(in float3 centerNormal, in float3 sampleNormal)
{
	const float dotProduct = dot(centerNormal, sampleNormal);
	return dotProduct > 0.f ? pow(dotProduct, 64.f) : 0.f;
}


float ComputeWorldLocationWeight(float3 centerWS, float3 normal, float3 sampleWS)
{
	const Plane centerPlane = Plane::Create(normal, centerWS);
	return 1.f - smoothstep(0.01f, 0.06f, centerPlane.Distance(sampleWS));
}

float ComputeRoughnessFilterStrength(in float roughness, in float reprojectionConfidence, in float historyFramesNum)
{
	const float maxStrength = lerp(8.f, 18.f, saturate(1.f - roughness));
	return clamp(1.f, maxStrength, 25.f * reprojectionConfidence * saturate(historyFramesNum / 8.f));
}

float ComputeRoughnessWeight(in float centerRoughness, in float sampleRoughness, in float roughnessFilterStrength)
{
	return saturate(1.f - abs(centerRoughness - sampleRoughness) * roughnessFilterStrength);
}

#endif // SR_DENOISING_COMMON_HLSLI
