#ifndef SR_DENOISING_COMMON_HLSLI
#define SR_DENOISING_COMMON_HLSLI


#define MAX_ACCUMULATED_FRAMES_NUM 38.f
#define FAST_HISTORY_MAX_ACCUMULATED_FRAMES_NUM 4.f

float GetGGXSpecularLobeTanHalfAngle(in float roughness, in float lobeFraction)
{
	const float a2 = Pow2(roughness);
	return a2 * lobeFraction / max( 1.0 - lobeFraction, SMALL_NUMBER);
}


float GetSpatialFilterMaxNormalAngleDiff(in float roughness, in float normalWeightsStrength, in float specularReprojectionConfidence, in float historyFramesNum, in float specularLobeAngleFraction, in float specularLobeAngleSlack)
{
	float filterStrength = saturate(historyFramesNum / 5.0);
	filterStrength *= lerp(specularReprojectionConfidence, 1.f, normalWeightsStrength);
	const float angleRelaxation = 0.9 + 0.1 * filterStrength;

	float maxAngle = atan(GetGGXSpecularLobeTanHalfAngle(roughness, specularLobeAngleFraction));
	maxAngle *= 10.0 - 9.0 * filterStrength;
	maxAngle += specularLobeAngleSlack;
	return min(PI * 0.5f, maxAngle / angleRelaxation);;
}


float ComputeSpecularNormalWeight(in float maxAngle, in float3 centerNormal, in float3 sampleNormal, in float3 centerV, in float3 sampleV)
{
	float surfacesCos = dot(centerNormal, sampleNormal);
	surfacesCos = min(surfacesCos, dot(centerV, sampleV));
	return 1.f - smoothstep(0.f, maxAngle, acos(surfacesCos));
}


float ComputeNormalWeight(in float3 centerNormal, in float3 sampleNormal)
{
	return saturate(pow(max(0.f, dot(centerNormal, sampleNormal)), 64.f));
}


float ComputeWorldLocationWeight(float3 centerWS, float3 normal, float3 sampleWS)
{
	const Plane centerPlane = Plane::Create(normal, centerWS);
	return centerPlane.Distance(sampleWS) < 0.05f ? 1.f : 0.f;
}

float ComputeRoughnessFilterStrength(in float roughness, in float reprojectionConfidence, in float historyFramesNum)
{
	const float maxStrength = lerp(2.f, 12.f, saturate(1.f - roughness));
	return clamp(1.f, maxStrength, 25.f * reprojectionConfidence * saturate(historyFramesNum / 8.f));
}

float ComputeRoughnessWeight(in float centerRoughness, in float sampleRoughness, in float roughnessFilterStrength)
{
	return saturate(1.f - abs(centerRoughness - sampleRoughness) * roughnessFilterStrength);
}

#endif // SR_DENOISING_COMMON_HLSLI
