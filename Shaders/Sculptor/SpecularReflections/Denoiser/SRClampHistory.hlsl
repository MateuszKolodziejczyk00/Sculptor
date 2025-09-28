#include "SculptorShader.hlsli"

[[descriptor_set(SRClampHistoryDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"
#include "Utils/ColorSpaces.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/SphericalHarmonics.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define USE_HISTORY_CLIPPING 1


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

groupshared half3 s_specularFastHistoryYCoCg[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];
groupshared half3 s_diffuseFastHistoryYCoCg[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];

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

		s_specularFastHistoryYCoCg[localPixel.x][localPixel.y] = half3(u_fastHistorySpecularTexture.Load(uint3(samplePixel, 0)).rgb);
		s_diffuseFastHistoryYCoCg[localPixel.x][localPixel.y]  = half3(u_fastHistoryDiffuseTexture.Load(uint3(samplePixel, 0)).rgb);
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
				const float3 specularFastHistoryYCoCg = s_specularFastHistoryYCoCg[input.localID.x + BORDER_SIZE + x][input.localID.y + BORDER_SIZE + y];
				const float3 diffuseFastHistoryYCoCg  = s_diffuseFastHistoryYCoCg[input.localID.x + BORDER_SIZE + x][input.localID.y + BORDER_SIZE + y];

				const float weight = 1.f;
				specularM1Sum += specularFastHistoryYCoCg * weight;
				specularM2Sum += Pow2(specularFastHistoryYCoCg) * weight;

				diffuseM1Sum += diffuseFastHistoryYCoCg * weight;
				diffuseM2Sum += Pow2(diffuseFastHistoryYCoCg) * weight;

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

		const SH2<float> specularY_SH2 = Float4ToSH2(u_specularY_SH2.Load(pixel));
		const SH2<float> diffuseY_SH2  = Float4ToSH2(u_diffuseY_SH2.Load(pixel));
		const float4 diffSpecCoCg      = u_diffSpecCoCg.Load(pixel);

		const float3 specularYCoCg = float3(specularY_SH2.Evaluate(normal), diffSpecCoCg.zw);
		const float3 diffuseYCoCg  = float3(diffuseY_SH2.Evaluate(normal), diffSpecCoCg.xy);

		const float3 clampedSpecularYCoCg = ApplyHistoryFix(specularYCoCg, specularFastHistoryMean - specularClampWindow, specularFastHistoryMean + specularClampWindow);
		const float3 clampedDiffuseYCoCg  = ApplyHistoryFix(diffuseYCoCg, diffuseFastHistoryMean - diffuseClampWindow, diffuseFastHistoryMean + diffuseClampWindow);

		if(specularYCoCg.x != clampedSpecularYCoCg.x)
		{
			const float ratio = specularYCoCg.x > 0.f ? clampedSpecularYCoCg.x / specularYCoCg.x : 1.f;
			u_specularY_SH2[pixel] = SH2ToFloat4(specularY_SH2) * ratio;
		}

		if (diffuseYCoCg.x != clampedDiffuseYCoCg.x)
		{
			const float ratio = diffuseYCoCg.x > 0.f ? clampedDiffuseYCoCg.x / diffuseYCoCg.x : 0.f;
			u_diffuseY_SH2[pixel] = SH2ToFloat4(diffuseY_SH2) * ratio;
		}

		u_diffSpecCoCg[pixel] = float4(clampedDiffuseYCoCg.yz, clampedSpecularYCoCg.yz);
	}
}
