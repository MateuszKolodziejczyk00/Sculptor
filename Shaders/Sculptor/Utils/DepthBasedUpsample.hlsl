#include "SculptorShader.hlsli"

[[shader_params(DepthBasedUpsampleConstants, PARAMS_DEPTH_BASED_UPSAMPLE)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


static const float depthDiffThreshold = 0.05f;


float ComputeSampleWeight(float sampleDepthDiff)
{
	return sampleDepthDiff <= depthDiffThreshold ? 1.f : 0.f;
}


[numthreads(8, 8, 1)]
void DepthBasedUpsampleCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	uint2 outputRes = PARAMS_DEPTH_BASED_UPSAMPLE->depthTexture.GetResolution();

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 outputPixelSize = rcp(float2(outputRes));
		const float2 outputUV = (float2(pixel) + 0.5f) * outputPixelSize;
		
		uint2 inputRes = PARAMS_DEPTH_BASED_UPSAMPLE->depthTextureHalfRes.GetResolution();
		const float2 inputPixelSize = rcp(float2(inputRes));

		const int2 inputPixel = pixel / 2 + (pixel & 1) - 1;
		const float2 inputUV = float2(inputPixel + 0.5f) * inputPixelSize;

		float4 bilinearWeights = { 0.1875f, 0.0625f, 0.1875f, 0.5625f };
		if((pixel.x & 1) == 0)
		{
			Swap(bilinearWeights.x, bilinearWeights.y);
			Swap(bilinearWeights.z, bilinearWeights.w);
		}
		if ((pixel.y & 1) == 0)
		{
			Swap(bilinearWeights.x, bilinearWeights.w);
			Swap(bilinearWeights.y, bilinearWeights.z);
		}

		const float4 inputDepths = PARAMS_DEPTH_BASED_UPSAMPLE->depthTextureHalfRes.Gather(BindlessSamplers::NearestClampEdge(), inputUV, 0);

		const int2 offsets[4] = { int2(0, 1), int2(1, 1), int2(1, 0), int2(0, 0) };

		const float outputDepth = PARAMS_DEPTH_BASED_UPSAMPLE->depthTexture.Load(int3(pixel, 0)).x;
		const float3 outputLocation = NDCToWorldSpace(float3(outputUV * 2.f - 1.f, outputDepth), VIEW->sceneView);

		float4 sampleDistances;
		[unroll]
		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			const float2 uv = inputUV + offsets[sampleIdx] * inputPixelSize;
			const float3 sampleLocation = NDCToWorldSpace(float3(uv * 2.f - 1.f, inputDepths[sampleIdx]), VIEW->sceneView);
			const float3 sampleNormal = OctahedronDecodeNormal(PARAMS_DEPTH_BASED_UPSAMPLE->normalsTextureHalfRes.Load(int3(inputPixel + offsets[sampleIdx], 0)));
			const Plane samplePlane = Plane::Create(sampleNormal, sampleLocation);
			sampleDistances[sampleIdx] = samplePlane.Distance(outputLocation);
		}

		const float minDistance = min(min(sampleDistances[0], sampleDistances[1]), min(sampleDistances[2], sampleDistances[3]));

		float4 weightsSum = 0.f;
		float4 input = 0.f;

		[unroll]
		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			float weight = ComputeSampleWeight(sampleDistances[sampleIdx] - minDistance);
			const float4 sample = PARAMS_DEPTH_BASED_UPSAMPLE->inputTexture.Load(int3(inputPixel + offsets[sampleIdx], 0));

			weight *= bilinearWeights[sampleIdx];

			if(PARAMS_DEPTH_BASED_UPSAMPLE->fireflyFilteringEnabled)
			{
				weight /= (1.f + Luminance(sample.rgb));
			}

			input += sample * weight;
			weightsSum += weight;
		}

		const float4 output = input / weightsSum;

		PARAMS_DEPTH_BASED_UPSAMPLE->outputTexture[pixel] = output;
	}
}
