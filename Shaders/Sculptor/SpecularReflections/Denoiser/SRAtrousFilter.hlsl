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

		const float centerLinearDepth = u_linearDepthTexture.Load(uint3(pixel, 0));

		if(isinf(centerLinearDepth))
		{
			return;
		}

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			u_outputTexture[pixel] = u_inputTexture.Load(uint3(pixel, 0));
			return;
		}
		
		const float lumStdDevMultiplier = 3.f;
		
		const float3 centerWS = LinearDepthToWS(u_sceneView, uv * 2.f - 1.f, centerLinearDepth);

		const float4 centerLuminanceHitDistance = u_inputTexture.Load(uint3(pixel, 0));
		const float lumCenter = Luminance(centerLuminanceHitDistance.rgb);

		float roughnessFilterStrength = 0.f;
		{
			const float accumulatedFrames = u_historyFramesNumTexture.Load(uint3(pixel, 0));
			const float specularReprojectionConfidence = u_reprojectionConfidenceTexture.Load(uint3(pixel, 0));
			roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularReprojectionConfidence, accumulatedFrames);
		}

		const float kernel[2] = { 3.f / 8.f, 2.f / 8.f };

		float weightSum = Pow2(kernel[0]);

		float3 luminanceSum = centerLuminanceHitDistance.rgb * weightSum;

		float lumSum = lumCenter * weightSum;
		float lumSquaredSum = Pow2(lumCenter) * weightSum;

		const float centerLuminanceStdDev = u_luminanceStdDevTexture[pixel];
		const float rcpLuminanceStdDev = 1.f / (centerLuminanceStdDev * lumStdDevMultiplier + 0.001f);

		const int radius = 1;

		float geometrySpatialCoherence = 0;

		[unroll]
		for (int y = -radius; y <= radius; ++y)
		{
			for (int x = -radius; x <= radius; ++x)
			{
				if (x == 0 && y == 0)
				{
					continue;
				}

				const int2 samplePixel = clamp(pixel + int2(x, y) * u_params.samplesOffset, int2(0, 0), int2(u_params.resolution - 1));
				float weight = kernel[abs(x)] * kernel[abs(y)];

				const float sampleLinearDepth = u_linearDepthTexture.Load(uint3(samplePixel, 0));
				const float sampleRoughness = u_roughnessTexture.Load(uint3(samplePixel, 0));
				const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(samplePixel, 0)));
				const float3 luminance = u_inputTexture.Load(uint3(samplePixel, 0)).rgb;

				if(isinf(sampleLinearDepth))
				{
					weight = 0.f;
				}

				const float3 sampleWS = LinearDepthToWS(u_sceneView, samplePixel * u_params.invResolution * 2.f - 1.f, sampleLinearDepth);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);
				weight *= dw;

				const float rw = ComputeRoughnessWeight(roughness, sampleRoughness, roughnessFilterStrength);
				weight *= rw;

				const float wn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
				weight *= wn;

				geometrySpatialCoherence += weight;

				const float lum = Luminance(luminance);

				const float lw = (abs(lumCenter - lum) * rcpLuminanceStdDev + 0.001f);
				weight *= exp(-lw);

				luminanceSum += luminance * weight;

				const float lumW = lum * weight;

				lumSum += lumW;
				lumSquaredSum += Pow2(lum) * weight;

				weightSum += weight;
			}
		}

		float prevCoherence = 0.f;
		if(u_params.samplesOffset != 1)
		{
			prevCoherence = u_geometryCoherenceTexture[pixel].x;
		}
		u_geometryCoherenceTexture[pixel] = prevCoherence + geometrySpatialCoherence;

		const float3 outLuminance = luminanceSum / weightSum;
		u_outputTexture[pixel] = float4(outLuminance, centerLuminanceHitDistance.w);

		lumSum /= weightSum;
		lumSquaredSum /= weightSum;

		const float variance = abs(lumSquaredSum - lumSum * lumSum);
		const float stdDev = sqrt(variance);

		u_luminanceStdDevTexture[pixel] = stdDev;
	}
}
