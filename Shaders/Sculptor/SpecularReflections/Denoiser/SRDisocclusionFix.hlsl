#include "SculptorShader.hlsli"

[[descriptor_set(SRDisocclusionFixDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/SphericalHarmonics.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void SRDisocclusionFixCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

	if(all(pixel < u_constants.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_constants.pixelSize;

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const uint specularHistoryLength =	u_specularHistoryLengthTexture.Load(int3(pixel, 0));
		const uint diffuseHistoryLength  = u_diffuseHistoryLengthTexture  .Load(int3(pixel, 0));

		const bool fixSpecular = specularHistoryLength < 3;
		const bool fixDiffuse  = diffuseHistoryLength < 5;

		if((!fixSpecular && !fixDiffuse) || roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			u_outSpecularY_SH2[pixel]  = u_inSpecularY_SH2.Load(uint3(pixel, 0));
			u_outDiffuseY_SH2[pixel]   = u_inDiffuseY_SH2.Load(uint3(pixel, 0));
			u_outDiffSpecCoCg[pixel]   = u_inDiffSpecCoCg.Load(uint3(pixel, 0));
			return;
		}
		
		const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), u_sceneView);

		const float centerWeight = kernel[0];

		const SH2<float> centerSpecularY_SH2 = Float4ToSH2(u_inSpecularY_SH2.Load(uint3(pixel, 0)));
		const SH2<float> centerDiffuseY_SH2  = Float4ToSH2(u_inDiffuseY_SH2.Load(uint3(pixel, 0)));
		const float4 centerDiffSpecCoCg      = u_inDiffSpecCoCg.Load(uint3(pixel, 0));

		SH2<float> specularY_SH2Sum = centerSpecularY_SH2 * centerWeight;
		SH2<float> diffuseY_SH2Sum  = centerDiffuseY_SH2 * centerWeight;
		float4 diffSpecCoCgSum      = centerDiffSpecCoCg * centerWeight;

		float specularWeightSum = centerWeight;
		float diffuseWeightSum  = centerWeight;

		for (int y = -2; y <= 2; ++y)
		{
			[unroll]
			for (int x = -2; x <= 2; ++x)
			{
				if(x == 0 && y == 0)
				{
					continue;
				}

				const int2 samplePixel = clamp(pixel + int2(x, y) * u_constants.filterStride, int2(0, 0), int2(u_constants.resolution));
				const float w = kernel[max(abs(x), abs(y))];

				const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(samplePixel, 0)));

				const float sampleDepth = u_depthTexture.Load(uint3(samplePixel, 0));

				if(sampleDepth < 0.000001f)
				{
					continue;
				}

				const float3 sampleWS = NDCToWorldSpaceNoJitter(float3((uv + float2(x, y) * u_constants.pixelSize) * 2.f - 1.f, sampleDepth), u_sceneView);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);

				const float weight = dw * w;

				const float4 sampleDiffSpecCoCg = u_inDiffSpecCoCg.Load(uint3(samplePixel, 0));

				if(fixSpecular)
				{
					const float swn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
					float specularWeight = weight * swn;

					const SH2<float> sampleSpecularY_SH2 = Float4ToSH2(u_inSpecularY_SH2.Load(uint3(samplePixel, 0)));

					const float specularLum = sampleSpecularY_SH2.Evaluate(sampleNormal);
					specularWeight /= specularLum + 1.f;

					specularY_SH2Sum = specularY_SH2Sum + sampleSpecularY_SH2 * specularWeight;
					diffSpecCoCgSum.zw += sampleDiffSpecCoCg.zw * specularWeight;
					specularWeightSum += specularWeight;
				}

				if(fixDiffuse)
				{
					const float dwn = ComputeDiffuseNormalWeight(normal, sampleNormal);
					float diffuseWeight = weight * dwn;

					const SH2<float> sampleDiffuseY_SH2 = Float4ToSH2(u_inDiffuseY_SH2.Load(uint3(samplePixel, 0)));

					const float diffuseLum = sampleDiffuseY_SH2.Evaluate(sampleNormal);
					diffuseWeight /= diffuseLum + 1.f;

					diffuseY_SH2Sum = diffuseY_SH2Sum + sampleDiffuseY_SH2 * diffuseWeight;
					diffSpecCoCgSum.xy += sampleDiffSpecCoCg.xy * diffuseWeight;
					diffuseWeightSum += diffuseWeight;
				}
			}
		}

		const float rcpSpecularWeightSum = 1.f / specularWeightSum;
		const float rcpDiffuseWeightSum = 1.f / diffuseWeightSum;

		const SH2<float> outSpecularY_SH2 = specularY_SH2Sum * rcpSpecularWeightSum;
		const SH2<float> outDiffuseY_SH2 = diffuseY_SH2Sum * rcpDiffuseWeightSum;
		const float4 outDiffSpecCoCg = diffSpecCoCgSum * float4(rcpDiffuseWeightSum, rcpDiffuseWeightSum, rcpSpecularWeightSum, rcpSpecularWeightSum);

		u_outSpecularY_SH2[pixel]  = SH2ToFloat4(outSpecularY_SH2);
		u_outDiffuseY_SH2[pixel]   = SH2ToFloat4(outDiffuseY_SH2);
		u_outDiffSpecCoCg[pixel]   = outDiffSpecCoCg;
	}
}
