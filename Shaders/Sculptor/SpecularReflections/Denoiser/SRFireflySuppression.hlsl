#include "SculptorShader.hlsli"

[[descriptor_set(SRFireflySuppressionDS, 0)]]

#include "Utils/SceneViewUtils.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define GROUP_SIZE 8

#define BORDER_SIZE 1

#define SHARED_SAMPLES_RES (GROUP_SIZE + 2 * BORDER_SIZE)

groupshared half3 s_specularLuminance[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];
groupshared half3 s_diffuseLuminance[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];


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

		s_specularLuminance[localPixel.x][localPixel.y] = half3(u_inSpecularHitDistTexture.Load(uint3(samplePixel, 0)).rgb);
		s_diffuseLuminance[localPixel.x][localPixel.y]  = half3(u_inDiffuseHitDistTexture.Load(uint3(samplePixel, 0)).rgb);
	}
}


[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void SRFireflySuppressionCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

	PreloadSharedSamples(input.groupID.xy, input.localID.xy);

	GroupMemoryBarrierWithGroupSync();

	if(all(pixel < u_constants.resolution))
	{
		float4 minSpecularLuminanceSample = 99999999.f;
		float4 maxSpecularLuminanceSample = -99999999.f;

		float4 minDiffuseLuminanceSample = 99999999.f;
		float4 maxDiffuseLuminanceSample = -99999999.f;

		float4 centerSpecular = u_inSpecularHitDistTexture.Load(uint3(pixel, 0));
		float4 centerDiffuse  = u_inDiffuseHitDistTexture.Load(uint3(pixel, 0));

		[unroll]
		for(int y = -1; y <= 1; ++y)
		{
			[unroll]
			for(int x = -1; x <= 1; ++x)
			{
				const int2 offset = int2(x, y);

				if(all(offset != int2(0, 0)))
				{
					const int2 samplePixel = clamp(pixel + offset, 0, u_constants.resolution - 1);

					// Specular
					{
						const float3 sampleSpecular = s_specularLuminance[input.localID.x + x + BORDER_SIZE][input.localID.y + y + BORDER_SIZE];
						const float sampleSpecularLum = Luminance(sampleSpecular.rgb);

						if(sampleSpecularLum > maxSpecularLuminanceSample.w)
						{
							maxSpecularLuminanceSample = float4(sampleSpecular, sampleSpecularLum);
						}
						if(sampleSpecularLum < minSpecularLuminanceSample.w)
						{
							minSpecularLuminanceSample = float4(sampleSpecular, sampleSpecularLum);
						}
					}

					// Diffuse
					{
						const float3 sampleDiffuse  = s_diffuseLuminance[input.localID.x + x + BORDER_SIZE][input.localID.y + y + BORDER_SIZE];
						const float sampleDiffuseLum  = Luminance(sampleDiffuse.rgb);

						if(sampleDiffuseLum > maxDiffuseLuminanceSample.w)
						{
							maxDiffuseLuminanceSample = float4(sampleDiffuse, sampleDiffuseLum);
						}
						if(sampleDiffuseLum < minDiffuseLuminanceSample.w)
						{
							minDiffuseLuminanceSample = float4(sampleDiffuse, sampleDiffuseLum);
						}
					}
				}
			}
		}

		// Specular
		{
			const float centerSpecularLum = Luminance(centerSpecular.rgb);
			if(centerSpecularLum > maxSpecularLuminanceSample.w)
			{
				centerSpecular.rgb = maxSpecularLuminanceSample.rgb;
			}
			if(centerSpecularLum < minSpecularLuminanceSample.w)
			{
				centerSpecular.rgb = minSpecularLuminanceSample.rgb;
			}

			u_outSpecularHitDistTexture[pixel] = centerSpecular;
		}

		// Diffuse
		{
			const float centerDiffuseLum = Luminance(centerDiffuse.rgb);
			if(centerDiffuseLum > maxDiffuseLuminanceSample.w)
			{
				centerDiffuse.rgb = maxDiffuseLuminanceSample.rgb;
			}
			if(centerDiffuseLum < minDiffuseLuminanceSample.w)
			{
				centerDiffuse.rgb = minDiffuseLuminanceSample.rgb;
			}

			u_outDiffuseHitDistTexture[pixel] = centerDiffuse;
		}
	}
}
