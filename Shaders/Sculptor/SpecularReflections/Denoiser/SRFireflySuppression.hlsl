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

groupshared half3 s_luminance[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];

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

		s_luminance[localPixel.x][localPixel.y] = half3(u_inputLuminanceHitDisTexture.Load(uint3(samplePixel, 0)).rgb);
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
		float4 minLuminanceSample = 99999999.f;
		float4 maxLuminanceSample = -99999999.f;

		float4 centerSample = u_inputLuminanceHitDisTexture.Load(uint3(pixel, 0));

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

					const float3 luminance = s_luminance[input.localID.x + x + BORDER_SIZE][input.localID.y + y + BORDER_SIZE];
					const float perceivedBrightness = Luminance(luminance.rgb);

					if(perceivedBrightness > maxLuminanceSample.w)
					{
						maxLuminanceSample = float4(luminance, perceivedBrightness);
					}
					if(perceivedBrightness < minLuminanceSample.w)
					{
						minLuminanceSample = float4(luminance, perceivedBrightness);
					}
				}
			}

			bool suppressed = false;
			const float centerPerceivedBrightness = Luminance(centerSample.rgb);
			if(centerPerceivedBrightness > maxLuminanceSample.w)
			{
				centerSample.rgb = maxLuminanceSample.rgb;
				suppressed = true;
			}
			if(centerPerceivedBrightness < minLuminanceSample.w)
			{
				centerSample.rgb = minLuminanceSample.rgb;
				suppressed = true;
			}

			if(suppressed)
			{
				u_outputLuminanceHitDisTexture[pixel] = centerSample;
			}
		}
	}
}
