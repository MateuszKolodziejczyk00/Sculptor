#include "SculptorShader.hlsli"

[[descriptor_set(SRClampHistoryDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"
#include "Utils/ColorSpaces.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


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

		s_specularFastHistoryYCoCg[localPixel.x][localPixel.y] = half3(RGBToYCoCg(u_fastHistorySpecularTexture.Load(uint3(samplePixel, 0)).rgb));
		s_diffuseFastHistoryYCoCg[localPixel.x][localPixel.y]  = half3(RGBToYCoCg(u_fastHistoryDiffuseTexture.Load(uint3(samplePixel, 0)).rgb));
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
		const float3 specularClampWindow         = 1.5f * specularFastHistoryStdDev;

		const float3 diffuseFastHistoryMean     = diffuseM1;
		const float3 diffuseFastHistoryVariance = abs(diffuseM2 - Pow2(diffuseM1));
		const float3 diffuseFastHistoryStdDev   = sqrt(diffuseFastHistoryVariance);
		const float3 diffuseClampWindow         = 1.5f * diffuseFastHistoryStdDev;

		const float4 specular                 = u_specularTexture.Load(pixel);
		const float3 accumulatedSpecularYCoCg = RGBToYCoCg(specular.rgb);
		const float3 clampedSpecularYCoCg     = clamp(accumulatedSpecularYCoCg, specularFastHistoryMean - specularClampWindow, specularFastHistoryMean + specularClampWindow);
		const float3 clampedSpecular          = YCoCgToRGB(clampedSpecularYCoCg);

		const float4 diffuse                 = u_diffuseTexture.Load(pixel);
		const float3 accumulatedDiffuseYCoCg = RGBToYCoCg(diffuse.rgb);
		const float3 clampedDiffuseYCoCg     = clamp(accumulatedDiffuseYCoCg, diffuseFastHistoryMean - diffuseClampWindow, diffuseFastHistoryMean + diffuseClampWindow);
		const float3 clampedDiffuse          = YCoCgToRGB(clampedDiffuseYCoCg);

		u_specularTexture[pixel] = float4(clampedSpecular, specular.w);
		u_diffuseTexture[pixel]  = float4(clampedDiffuse, diffuse.w);
	}
}
