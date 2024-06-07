#include "SculptorShader.hlsli"

[[descriptor_set(SRClampHistoryDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"
#include "Utils/ColorSpaces.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define GROUP_SIZE 8

#define BORDER_SIZE 2

#define SHARED_SAMPLES_RES (GROUP_SIZE + 2 * BORDER_SIZE)

groupshared half3 s_fastHistoryYCoCg[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];

void PreloadSharedSamples(in uint2 groupID, in uint2 localID)
{
	const uint localThreadID = localID.y * GROUP_SIZE + localID.x;

	const int2 groupOffset = groupID * GROUP_SIZE - BORDER_SIZE;

	const uint sharedSamplesNum = SHARED_SAMPLES_RES * SHARED_SAMPLES_RES;

	const int2 maxPixel = u_constants.resolution - 1;

	for(uint pixelIdx = localThreadID; pixelIdx < sharedSamplesNum; pixelIdx += GROUP_SIZE * GROUP_SIZE)
	{
		int2 localPixel = 0;
		localPixel.y = pixelIdx / SHARED_SAMPLES_RES;
		localPixel.x = pixelIdx - localPixel.y * SHARED_SAMPLES_RES;

		const int2 samplePixel = clamp(groupOffset + localPixel, 0, maxPixel);

		s_fastHistoryYCoCg[localPixel.x][localPixel.y] = half3(RGBToYCoCg(u_fastHistoryLuminanceTexture.Load(uint3(samplePixel, 0)).rgb));
	}
}


[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void SRClampHistoryCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

	PreloadSharedSamples(input.groupID.xy, input.localID.xy);

	GroupMemoryBarrierWithGroupSync();

	if(all(pixel < u_constants.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_constants.pixelSize;

		const float3 normal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));
	
		const uint accumulatedSamplesNum = u_accumulatedSamplesNumTexture.Load(pixel);
		if(accumulatedSamplesNum == 0 ||accumulatedSamplesNum >= MAX_ACCUMULATED_FRAMES_NUM || roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			return;
		}

		float3 m1Sum = 0.f;
		float3 m2Sum = 0.f;
		float weightSum = 0.000001f;
		
		for(int y = -2; y <= 2; ++y)
		{
			for(int x = -2; x <= 2; ++x)
			{
				const float3 fastHistoryYCoCg = s_fastHistoryYCoCg[input.localID.x + BORDER_SIZE + x][input.localID.y + BORDER_SIZE + y];

				const float weight = 1.f;
				m1Sum += fastHistoryYCoCg * weight;
				m2Sum += Pow2(fastHistoryYCoCg) * weight;

				weightSum += weight;
			}
		}

		const float weightSumRcp = rcp(weightSum);
		const float3 m1 = m1Sum * weightSumRcp;
		const float3 m2 = m2Sum * weightSumRcp;

		const float3 fastHistoryMean = m1;
		const float3 fastHistoryVariance = abs(m2 - Pow2(m1));

		const float3 fastHistoryStdDev = sqrt(fastHistoryVariance);

		const float3 clampWindow = 1.6f * fastHistoryStdDev;

		const float4 luminanceAndHitDistance = u_luminanceAndHitDistanceTexture.Load(pixel);
		const float3 accumulatedHistoryYCoCg = RGBToYCoCg(luminanceAndHitDistance.rgb);

		const float3 clampedHistoryYCoCg = clamp(accumulatedHistoryYCoCg, fastHistoryMean - clampWindow, fastHistoryMean + clampWindow);
		const float3 clampedHistory = YCoCgToRGB(clampedHistoryYCoCg);

		u_luminanceAndHitDistanceTexture[pixel] = float4(clampedHistory, luminanceAndHitDistance.a);
	}
}
