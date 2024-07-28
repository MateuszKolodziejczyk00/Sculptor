#include "SculptorShader.hlsli"

[[descriptor_set(SRATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};

[numthreads(8, 4, 1)]
void SRATrousFilterCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	if(all(pixel < u_params.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_params.invResolution;

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));

		if(centerDepth == 0.0f)
		{
			return;
		}

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			u_outputTexture[pixel] = u_inputTexture.Load(uint3(pixel, 0));
			return;
		}
		
		const float lumStdDevMultiplier = lerp(1.f, 3.f, roughness);
		
		const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), u_sceneView);
		
		const float3 centerV = normalize(u_sceneView.viewLocation - centerWS);

		const float4 centerLuminanceHitDistance = u_inputTexture.Load(uint3(pixel, 0));
		const float lumCenter = Luminance(centerLuminanceHitDistance.rgb);

		float maxNormalAngleDiff = 0.f;
		float roughnessFilterStrength = 0.f;
		{
			const float accumulatedFrames = u_historyFramesNumTexture.Load(uint3(pixel, 0));
			const float specularReprojectionConfidence = u_reprojectionConfidenceTexture.Load(uint3(pixel, 0));
			const float normalEdgeStoppingStrength = 1.0f;
			const float specularLobeAngleFraction = 0.3f;
			const float specularLobeAngleSlack = 0.15f * 0.2f * PI * (accumulatedFrames / MAX_ACCUMULATED_FRAMES_NUM);
			maxNormalAngleDiff = GetSpatialFilterMaxNormalAngleDiff(roughness, normalEdgeStoppingStrength, specularReprojectionConfidence, accumulatedFrames, specularLobeAngleFraction, specularLobeAngleSlack);
			roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularReprojectionConfidence, accumulatedFrames);
		}

		float3 luminanceSum = 0.0f;
		
		float lumSum = 0.0f;
		float lumSquaredSum = 0.0f;

		float weightSum = 0.000001f;

		const float centerLuminanceStdDev = u_luminanceStdDevTexture[pixel];
		const float rcpLuminanceStdDev = 1.f / (centerLuminanceStdDev * lumStdDevMultiplier + 0.001f);

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		for (int y = -2; y <= 2; ++y)
		{
			for (int x = -2; x <= 2; ++x)
			{
				const int2 samplePixel = clamp(pixel + int2(x, y) * u_params.samplesOffset, int2(0, 0), int2(u_params.resolution - 1));
				float weight = kernel[abs(x)] * kernel[abs(y)];

				const float sampleDepth = u_depthTexture.Load(uint3(samplePixel, 0));

				if(sampleDepth == 0.0f)
				{
					weight = 0.f;
				}

				const float3 sampleWS = NDCToWorldSpaceNoJitter(float3(samplePixel * u_params.invResolution * 2.f - 1.f, sampleDepth), u_sceneView);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);
				weight *= dw;

				const float sampleRoughness = u_roughnessTexture.Load(uint3(samplePixel, 0));
				const float rw = ComputeRoughnessWeight(roughness, sampleRoughness, roughnessFilterStrength);
				weight *= rw;

				const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(samplePixel, 0)));
				const float3 sampleV = normalize(u_sceneView.viewLocation - sampleWS);
				const float wn = ComputeSpecularNormalWeight(maxNormalAngleDiff, normal, sampleNormal, centerV, sampleV);
				weight *= wn;

				const float3 luminance = u_inputTexture.Load(uint3(samplePixel, 0)).rgb;
				const float lum = Luminance(luminance);
				const float lw = abs(lumCenter - lum) * rcpLuminanceStdDev + 0.001f;
				weight *= exp(-lw);

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
