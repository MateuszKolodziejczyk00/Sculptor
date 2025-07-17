#include "SculptorShader.hlsli"

[[descriptor_set(SRClampHistoryDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"
#include "Utils/ColorSpaces.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define USE_YCOCG 1
#define USE_HISTORY_CLIPPING 1


float3 RGBToWorkingCS(in float3 rgb)
{
#if USE_YCOCG
	return RGBToYCoCg(rgb);
#else
	return rgb;
#endif // USE_YCOCG
}

float3 WorkingCSToRGB(in float3 workingCS)
{
#if USE_YCOCG
	return YCoCgToRGB(workingCS);
#else
	return workingCS;
#endif // USE_YCOCG
}


float3 ApplyHistoryFix(in float3 value, in float3 minValue, in float3 maxValue)
{
#if USE_HISTORY_CLIPPING
	return ClipAABB(minValue, maxValue, value);
#else
	return clamp(value, minValue, maxValue);
#endif // USE_HISTORY_CLIPPING
}


#define GROUP_SIZE 8

#define BORDER_SIZE 2

#define SHARED_SAMPLES_RES (GROUP_SIZE + 2 * BORDER_SIZE)

groupshared half3 s_specularFastHistoryWorkingCS[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];
groupshared half3 s_diffuseFastHistoryWorkingCS[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];

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

		s_specularFastHistoryWorkingCS[localPixel.x][localPixel.y] = half3(RGBToWorkingCS(u_fastHistorySpecularTexture.Load(uint3(samplePixel, 0)).rgb));
		s_diffuseFastHistoryWorkingCS[localPixel.x][localPixel.y]  = half3(RGBToWorkingCS(u_fastHistoryDiffuseTexture.Load(uint3(samplePixel, 0)).rgb));
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

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));
		if(centerDepth == 0.f)
		{
			return;
		}

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));
	
		if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			return;
		}

		float3 specularM1Sum = 0.f;
		float3 specularM2Sum = 0.f;

		float3 diffuseM1Sum = 0.f;
		float3 diffuseM2Sum = 0.f;

		float weightSum = 0.000001f;
		
		for(int y = -2; y <= 2; ++y)
		{
			for(int x = -2; x <= 2; ++x)
			{
				const float3 specularFastHistoryWorkingCS = s_specularFastHistoryWorkingCS[input.localID.x + BORDER_SIZE + x][input.localID.y + BORDER_SIZE + y];
				const float3 diffuseFastHistoryWorkingCS  = s_diffuseFastHistoryWorkingCS[input.localID.x + BORDER_SIZE + x][input.localID.y + BORDER_SIZE + y];

				const float weight = 1.f;
				specularM1Sum += specularFastHistoryWorkingCS * weight;
				specularM2Sum += Pow2(specularFastHistoryWorkingCS) * weight;

				diffuseM1Sum += diffuseFastHistoryWorkingCS * weight;
				diffuseM2Sum += Pow2(diffuseFastHistoryWorkingCS) * weight;

				weightSum += weight;
			}
		}

		const float weightSumRcp = rcp(weightSum);

		const float3 specularM1 = specularM1Sum * weightSumRcp;
		const float3 specularM2 = specularM2Sum * weightSumRcp;

		const float3 diffuseM1 = diffuseM1Sum * weightSumRcp;
		const float3 diffuseM2 = diffuseM2Sum * weightSumRcp;

		const float3 specularFastHistoryMean     = specularM1;
		const float3 specularFastHistoryVariance = abs(specularM2 - Pow2(specularM1));
		const float3 specularFastHistoryStdDev   = sqrt(specularFastHistoryVariance);
		const float3 specularClampWindow         = 1.3f * specularFastHistoryStdDev;

		const float3 diffuseFastHistoryMean     = diffuseM1;
		const float3 diffuseFastHistoryVariance = abs(diffuseM2 - Pow2(diffuseM1));
		const float3 diffuseFastHistoryStdDev   = sqrt(diffuseFastHistoryVariance);
		const float3 diffuseClampWindow         = 1.5f * diffuseFastHistoryStdDev;

		const float4 specular                     = u_specularTexture.Load(pixel);
		const float3 accumulatedSpecularWorkingCS = RGBToWorkingCS(specular.rgb);
		const float3 clampedSpecularWorkingCS     = ApplyHistoryFix(accumulatedSpecularWorkingCS, specularFastHistoryMean - specularClampWindow, specularFastHistoryMean + specularClampWindow);
		const float3 clampedSpecular              = WorkingCSToRGB(clampedSpecularWorkingCS);

		const float4 diffuse                     = u_diffuseTexture.Load(pixel);
		const float3 accumulatedDiffuseWorkingCS = RGBToWorkingCS(diffuse.rgb);
		const float3 clampedDiffuseWorkingCS     = ApplyHistoryFix(accumulatedDiffuseWorkingCS, diffuseFastHistoryMean - diffuseClampWindow, diffuseFastHistoryMean + diffuseClampWindow);
		const float3 clampedDiffuse              = WorkingCSToRGB(clampedDiffuseWorkingCS);

		u_specularTexture[pixel] = float4(clampedSpecular, specular.w);
		u_diffuseTexture[pixel]  = float4(clampedDiffuse, diffuse.w);
	}
}
