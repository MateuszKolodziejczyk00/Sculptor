#include "SculptorShader.hlsli"

[[descriptor_set(SpatialATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


struct CachedSample
{
	float depth;
	half3 normal;
	half  value;
};

groupshared CachedSample cachedSamples[24][24];
groupshared uint needsProcessingMask;


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


float NormalWeight(half3 centerNormal, half3 sampleNomral)
{
	return pow(max(0.f, float(dot(centerNormal, sampleNomral))), 8.f);
}


float WorldLocationWeight(float3 centerWS, float3 normal, float3 sampleWS)
{
	const Plane centerPlane = Plane::Create(normal, centerWS);
	return centerPlane.Distance(sampleWS) < 0.03f ? 1.f : 0.f;
}


void CacheLocalSamples(in uint2 groupID, in uint2 localID)
{
	const int2 groupBasePixel = groupID * 8 - int2(8, 8);

	uint2 outputRes;
	u_inputTexture.GetDimensions(outputRes.x, outputRes.y);

	for(int i = 0; i <= 9; ++i)
	{

		int2 offset;
		offset.x = i / 3;
		offset.y = i - offset.x * 3;
		const int2 localPixel = localID * 3 + offset;
		const int3 samplePixel = int3(clamp(groupBasePixel + localPixel, 0, outputRes - 1), 0);

		CachedSample sample;
		sample.depth  = u_depthTexture.Load(samplePixel).x;
		sample.normal = half3(OctahedronDecodeNormal(u_normalsTexture.Load(samplePixel)));
		sample.value  = half(u_inputTexture.Load(samplePixel));

		cachedSamples[localPixel.x][localPixel.y] = sample;
	}
}


[numthreads(8, 8, 1)]
void SpatialATrousFilterCS(CS_INPUT input)
{
	uint2 outputRes;
	u_inputTexture.GetDimensions(outputRes.x, outputRes.y);

	const uint2 pixel = min(input.globalID.xy, outputRes - 1);
	
	needsProcessingMask = 0;

	GroupMemoryBarrierWithGroupSync();

	const float variance = u_varianceTexture[pixel];
	const float stdDev = sqrt(variance);

	const bool needsProcessing = stdDev > 0.001f;

	InterlockedOr(needsProcessingMask, WaveActiveBallot(needsProcessing).x);

	GroupMemoryBarrierWithGroupSync();

	if(needsProcessingMask > 0)
	{
		CacheLocalSamples(input.groupID.xy, input.localID.xy);
	}

	const float visibilityCenter = u_inputTexture[pixel];

	if(!needsProcessing)
	{
		u_outputTexture[pixel] = visibilityCenter;
		return;
	}

	const float2 pixelSize = rcp(float2(outputRes));
	const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

	GroupMemoryBarrierWithGroupSync();

	const CachedSample centerSample = cachedSamples[8 + input.localID.x][8 + input.localID.y];

	const float3 ndc = float3(uv * 2.f - 1.f, centerSample.depth);
	const float3 centerWS = NDCToWorldSpaceNoJitter(ndc, u_sceneView);

	const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

	float valueSum = 0.0f;
	float valueSqSum = 0.0f;
	float weightSum = 0.0f;
	for (int x = -2; x <= 2; ++x)
	{
		[unroll]
		for (int y = -2; y <= 2; ++y)
		{
			const int k = max(abs(x), abs(y));
			float w = kernel[k];

			const int2 offset = int2(x, y) * u_params.samplesOffset;

			const int2 localSampleID = input.localID.xy + offset + 8;
			const bool isValidSample = all(localSampleID >= 0) && all(localSampleID < 24);
			SPT_CHECK_MSG(isValidSample, L"InvalidSample ({}) = ({}) + ({}) + 8", localSampleID, input.localID.xy, offset);
			const CachedSample sample = cachedSamples[localSampleID.x][localSampleID.y];

			const float2 sampleUV = saturate(uv + offset * pixelSize);

			const float3 sampleNDC = float3(sampleUV * 2.f - 1.f, sample.depth);
			const float3 sampleWS = NDCToWorldSpaceNoJitter(sampleNDC, u_sceneView);

			const float wn = NormalWeight(centerSample.normal, sample.normal);
			const float wl = WorldLocationWeight(centerWS, centerSample.normal, sampleWS);

			const float sampleVisibility = sample.value;

			const float lw = abs(visibilityCenter - sampleVisibility) / (1.5f * stdDev + 0.001f);
			
			const float weight = exp(-lw) * w * wn * wl;

			valueSum   += sampleVisibility * weight;
			valueSqSum += Pow2(sampleVisibility) * weight;
			weightSum  += weight;
		}
	}

	weightSum += 0.001f;

	const float mean = valueSum / weightSum;
	u_outputTexture[pixel] = mean;
		
	const float newVariance = abs(valueSqSum / weightSum - Pow2(mean));
	u_varianceTexture[pixel] = newVariance;
}
