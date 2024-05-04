#include "SculptorShader.hlsli"

[[descriptor_set(DepthBasedUpsampleDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"


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
	
	uint2 outputRes;
	u_depthTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 outputPixelSize = rcp(float2(outputRes));
		const float2 outputUV = (float2(pixel) + 0.5f) * outputPixelSize;
		
		uint2 inputRes;
		u_depthTextureHalfRes.GetDimensions(inputRes.x, inputRes.y);
		const float2 inputPixelSize = rcp(float2(inputRes));

		const int2 inputPixel = pixel / 2 + (pixel & 1) - 1;
		const float2 inputUV = float2(inputPixel + 0.5f) * inputPixelSize;

		const float4 inputDepths = u_depthTextureHalfRes.Gather(u_nearestSampler, inputUV, 0);

		const int2 offsets[4] = { int2(0, 1), int2(1, 1), int2(1, 0), int2(0, 0) };

		const float outputDepth = u_depthTexture.Load(int3(pixel, 0)).x;
		const float3 outputLocation = NDCToWorldSpace(float3(outputUV * 2.f - 1.f, outputDepth), u_sceneView);

		float4 sampleDistances;
		[unroll]
		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			const float2 uv = inputUV + offsets[sampleIdx] * inputPixelSize;
			const float3 sampleLocation = NDCToWorldSpace(float3(uv * 2.f - 1.f, inputDepths[sampleIdx]), u_sceneView);
			const float3 sampleNormal = u_normalsTextureHalfRes.Load(int3(inputPixel + offsets[sampleIdx], 0)) * 2.f - 1.f;
			const Plane samplePlane = Plane::Create(sampleNormal, sampleLocation);
			sampleDistances[sampleIdx] = samplePlane.Distance(outputLocation);
		}

		const float minDistance = min(min(sampleDistances[0], sampleDistances[1]), min(sampleDistances[2], sampleDistances[3]));

		float4 weightsSum = 0.f;
		float4 input = 0.f;
		[unroll]
		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			const float weight = ComputeSampleWeight(sampleDistances[sampleIdx] - minDistance);
			input += u_inputTexture.Load(int3(inputPixel + offsets[sampleIdx], 0)) * weight;
			weightsSum += weight;
		}

		const float4 output = input / weightsSum;

		u_outputTexture[pixel] = output;
	}
}
