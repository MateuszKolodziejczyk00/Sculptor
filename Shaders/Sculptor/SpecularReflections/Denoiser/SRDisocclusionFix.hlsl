#include "SculptorShader.hlsli"

[[descriptor_set(SRDisocclusionFixDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 4, 1)]
void SRDisocclusionFixCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

	if(all(pixel < u_constants.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_constants.pixelSize;

		const float3 normal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));

		const uint accumulatedSamplesNum = u_accumulatedSamplesNumTexture.Load(int3(pixel, 0));
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(accumulatedSamplesNum > 3 || roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			u_outputLuminanceTexture[pixel] = u_inputLuminanceTexture.Load(uint3(pixel, 0));
			return;
		}
		
		const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), u_sceneView);

		const float4 centerLuminanceHitDistance = u_inputLuminanceTexture.Load(uint3(pixel, 0));
		const float3 centerV = normalize(u_sceneView.viewLocation - centerWS);

		const float accumulatedFrames = accumulatedSamplesNum;
		const float specularReprojectionConfidence = 0.2f;
		const float normalEdgeStoppingStrength = 0.5f;
		const float specularLobeAngleFraction = 0.5f;
		const float specularLobeAngleSlack = 0.15f * 0.5f * PI;

		const float maxNormalAngleDiff = GetSpatialFilterMaxNormalAngleDiff(roughness, normalEdgeStoppingStrength, specularReprojectionConfidence, accumulatedFrames, specularLobeAngleFraction, specularLobeAngleSlack);

		float3 luminanceSum = 0.0f;
		float weightSum = 0.000001f;

		for (int y = -2; y <= 2; ++y)
		{
			[unroll]
			for (int x = -2; x <= 2; ++x)
			{
				const int2 samplePixel = clamp(pixel + int2(x, y) * u_constants.filterStride, int2(0, 0), int2(u_constants.resolution));
				const float w = kernel[abs(x)] * kernel[abs(y)];

				const float3 sampleNormal = u_normalsTexture.Load(uint3(samplePixel, 0)).xyz * 2.f - 1.f;

				const float sampleDepth = u_depthTexture.Load(uint3(samplePixel, 0));

				if(sampleDepth < 0.000001f)
				{
					continue;
				}

				const float3 sampleWS = NDCToWorldSpaceNoJitter(float3((uv + float2(x, y) * u_constants.pixelSize) * 2.f - 1.f, sampleDepth), u_sceneView);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);

				const float3 luminance = u_inputLuminanceTexture.Load(uint3(samplePixel, 0)).rgb;
				const float lum = Luminance(luminance);

				const float3 sampleV = normalize(u_sceneView.viewLocation - sampleWS);
				const float wn = ComputeSpecularNormalWeight(maxNormalAngleDiff, normal, sampleNormal, centerV, sampleV);
				
				const float weight = wn * dw * w;

				luminanceSum += luminance * weight;

				const float lumW = lum * weight;

				weightSum += weight;
			}
		}

		float3 outLuminance = luminanceSum / weightSum;
		if(any(isnan(outLuminance)))
		{
			outLuminance = centerLuminanceHitDistance.rgb;
		}
		u_outputLuminanceTexture[pixel] = float4(outLuminance, centerLuminanceHitDistance.w);
	}
}
