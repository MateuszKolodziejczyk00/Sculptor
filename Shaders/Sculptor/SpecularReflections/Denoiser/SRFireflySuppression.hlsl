#include "SculptorShader.hlsli"

[[descriptor_set(SRFireflySuppressionDS, 0)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/SphericalHarmonics.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


#define GROUP_SIZE 8

#define BORDER_SIZE 1

#define SHARED_SAMPLES_RES (GROUP_SIZE + 2 * BORDER_SIZE)

groupshared SH2<half> gs_specularY_SH2[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];
groupshared SH2<half> gs_diffuseY_SH2[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];
groupshared half4 gs_diffSpecCoCg[SHARED_SAMPLES_RES][SHARED_SAMPLES_RES];


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

		gs_specularY_SH2[localPixel.x][localPixel.y] = Half4ToSH2(half4(u_inSpecularY_SH2.Load(uint3(samplePixel, 0))));
		gs_diffuseY_SH2[localPixel.x][localPixel.y]  = Half4ToSH2(half4(u_inDiffuseY_SH2.Load(uint3(samplePixel, 0))));
		gs_diffSpecCoCg[localPixel.x][localPixel.y]  = half4(u_inDiffSpecCoCg.Load(uint3(samplePixel, 0)));
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
		float minSpecY = 99999999.f;
		SH2<half> minSpecSH;
		half2 minSpecCoCg;
		float maxSpecY = -99999999.f;
		SH2<half> maxSpecSH;
		half2 maxSpecCoCg;

		float minDiffY = 99999999.f;
		SH2<half> minDiffSH;
		half2 minDiffCoCg;
		float maxDiffY = -99999999.f;
		SH2<half> maxDiffSH;
		half2 maxDiffCoCg;


		const float3 centerNormal = OctahedronDecodeNormal(u_normals.Load(uint3(pixel, 0)));

		SH2<float> centerSpecular     = Float4ToSH2(u_inSpecularY_SH2.Load(uint3(pixel, 0)));
		SH2<float> centerDiffuse      = Float4ToSH2(u_inDiffuseY_SH2.Load(uint3(pixel, 0)));
		float4     centerDiffSpecCoCg = u_inDiffSpecCoCg.Load(uint3(pixel, 0));

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

					const half4 sampleDiffSpecCoCg = gs_diffSpecCoCg[input.localID.x + x + BORDER_SIZE][input.localID.y + y + BORDER_SIZE];

					// Specular
					{
						const SH2<half> sample = gs_specularY_SH2[input.localID.x + x + BORDER_SIZE][input.localID.y + y + BORDER_SIZE];
						const float sampleSpecY = sample.Evaluate(centerNormal);

						if(sampleSpecY > maxSpecY)
						{
							maxSpecY = sampleSpecY;
							maxSpecSH = sample;
							maxSpecCoCg = sampleDiffSpecCoCg.zw;
						}
						if(sampleSpecY < minSpecY)
						{
							minSpecY = sampleSpecY;
							minSpecSH = sample;
							minSpecCoCg = sampleDiffSpecCoCg.zw;
						}
					}

					// Diffuse
					{
						const SH2<half> sample = gs_diffuseY_SH2[input.localID.x + x + BORDER_SIZE][input.localID.y + y + BORDER_SIZE];
						const float sampleDiffY = sample.Evaluate(centerNormal);

						if(sampleDiffY > maxDiffY)
						{
							maxDiffY = sampleDiffY;
							maxDiffSH = sample;
							maxDiffCoCg = sampleDiffSpecCoCg.xy;
						}
						if(sampleDiffY < minDiffY)
						{
							minDiffY = sampleDiffY;
							minDiffSH = sample;
							minDiffCoCg = sampleDiffSpecCoCg.xy;
						}
					}
				}
			}
		}

		// Specular
		{
			const float centerSpecY = centerSpecular.Evaluate(centerNormal);
			if(centerSpecY > maxSpecY)
			{
				centerSpecular = SH2HalfToFloat(maxSpecSH);
				centerDiffSpecCoCg.zw = maxSpecCoCg;

			}
			if(centerSpecY < minSpecY)
			{
				centerSpecular = SH2HalfToFloat(minSpecSH);
				centerDiffSpecCoCg.zw = minSpecCoCg;
			}

			u_outSpecularY_SH2[pixel] = SH2ToFloat4(centerSpecular);
		}

		// Diffuse
		{
			const float centerDiffY = centerDiffuse.Evaluate(centerNormal);
			if(centerDiffY > maxDiffY)
			{
				centerDiffuse = SH2HalfToFloat(maxDiffSH);
				centerDiffSpecCoCg.xy = maxDiffCoCg;
			}
			if(centerDiffY < minDiffY)
			{
				centerDiffuse = SH2HalfToFloat(minDiffSH);
				centerDiffSpecCoCg.xy = minDiffCoCg;
			}

			u_outDiffuseY_SH2[pixel] = SH2ToFloat4(centerDiffuse);
		}

		u_outDiffSpecCoCg[pixel] = centerDiffSpecCoCg;
	}
}
