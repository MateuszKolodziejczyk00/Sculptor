#include "SculptorShader.hlsli"

[[descriptor_set(SRATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void SRATrousFilterCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float3 normal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));

		if(centerDepth < 0.000001f)
		{
			return;
		}

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS || roughness > GLOSSY_TRACE_MAX_ROUGHNESS)
		{
			u_outputTexture[pixel] = u_inputTexture.Load(uint3(pixel, 0));
			return;
		}
		
		const float lumStdDevMultiplier = lerp(1.f, 3.f, roughness);
		
		const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), u_sceneView);
		
		const float3 centerV = normalize(u_sceneView.viewLocation - centerWS);

		const float centerLuminanceStdDev = u_luminanceStdDevTexture[pixel];

		const float4 centerLuminanceHitDistance = u_inputTexture.Load(uint3(pixel, 0));
		const float lumCenter = Luminance(centerLuminanceHitDistance.rgb);

		float3 luminanceSum = 0.0f;
		
		float lumSum = 0.0f;
		float lumSquaredSum = 0.0f;

		float weightSum = 0.000001f;

		const float accumulatedFrames = u_historyFramesNumTexture.Load(uint3(pixel, 0));
		const float specularReprojectionConfidence = u_reprojectionConfidenceTexture.Load(uint3(pixel, 0));
		const float normalEdgeStoppingStrength = 1.0f;
		const float specularLobeAngleFraction = 0.3f;
		const float specularLobeAngleSlack = 0.15f * 0.2f * PI * (accumulatedFrames / MAX_ACCUMULATED_FRAMES_NUM);

		const float maxNormalAngleDiff = GetSpatialFilterMaxNormalAngleDiff(roughness, normalEdgeStoppingStrength, specularReprojectionConfidence, accumulatedFrames, specularLobeAngleFraction, specularLobeAngleSlack);

		const float roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularReprojectionConfidence, accumulatedFrames);

		for (int y = -2; y <= 2; ++y)
		{
			[unroll]
			for (int x = -2; x <= 2; ++x)
			{
				const int2 samplePixel = clamp(pixel + int2(x, y) * u_params.samplesOffset, int2(0, 0), int2(outputRes));
				const float w = kernel[abs(x)] * kernel[abs(y)];

				const float3 sampleNormal = u_normalsTexture.Load(uint3(samplePixel, 0)).xyz * 2.f - 1.f;

				const float sampleDepth = u_depthTexture.Load(uint3(samplePixel, 0));

				if(sampleDepth < 0.000001f)
				{
					continue;
				}

				const float3 sampleWS = NDCToWorldSpaceNoJitter(float3((uv + float2(x, y) * rcp(float2(outputRes))) * 2.f - 1.f, sampleDepth), u_sceneView);

				const float3 sampleV = normalize(u_sceneView.viewLocation - sampleWS);

				const float3 luminance = u_inputTexture.Load(uint3(samplePixel, 0)).rgb;
				const float lum = Luminance(luminance);

				const float wn = ComputeSpecularNormalWeight(maxNormalAngleDiff, normal, sampleNormal, centerV, sampleV);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);
				const float lw = abs(lumCenter - lum) / (centerLuminanceStdDev * lumStdDevMultiplier + 0.001f) + 0.001f;

				const float sampleRoughness = u_roughnessTexture.Load(uint3(samplePixel, 0));
				const float rw = ComputeRoughnessWeight(roughness, sampleRoughness, roughnessFilterStrength);
				
				const float weight = exp(-lw) * wn * dw * rw * w;

				luminanceSum += luminance * weight;

				const float lumW = lum * weight;

				lumSum += lumW;
				lumSquaredSum += lumW * lumW;

				weightSum += weight;
			}
		}

		weightSum += 0.00001f;

		const float3 outLuminance = luminanceSum / weightSum;
		u_outputTexture[pixel] = float4(outLuminance, centerLuminanceHitDistance.w);

		lumSum /= weightSum;
		lumSquaredSum /= weightSum;

		const float variance = abs(lumSquaredSum - lumSum * lumSum);
		const float stdDev = sqrt(variance);

		u_luminanceStdDevTexture[pixel] = stdDev;
	}
}
