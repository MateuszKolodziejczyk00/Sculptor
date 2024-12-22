#include "SculptorShader.hlsli"

[[descriptor_set(SRDisocclusionFixDS, 0)]]
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
void SRDisocclusionFixCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;

	if(all(pixel < u_constants.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_constants.pixelSize;

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = u_depthTexture.Load(uint3(pixel, 0));

		const uint accumulatedSamplesNum = u_accumulatedSamplesNumTexture.Load(int3(pixel, 0));
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(accumulatedSamplesNum > 3 || roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			u_rwDiffuseTexture[pixel]  = u_diffuseTexture.Load(uint3(pixel, 0));
			u_rwSpecularTexture[pixel] = u_specularTexture.Load(uint3(pixel, 0));
			return;
		}
		
		const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), u_sceneView);

		const float centerWeight = kernel[0];

		float4 centerSpecular = u_specularTexture.Load(uint3(pixel, 0));
		centerSpecular.rgb /= (Luminance(centerSpecular.rgb) + 1.f);

		float4 centerDiffuse  = u_diffuseTexture.Load(uint3(pixel, 0));
		centerDiffuse.rgb /= (Luminance(centerDiffuse.rgb) + 1.f);

		float3 specularSum = centerSpecular.rgb * centerWeight;
		float3 diffuseSum  = centerDiffuse.rgb  * centerWeight;

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

				const float swn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
				const float specularWeight = weight * swn;

				float3 specular = u_specularTexture.Load(uint3(samplePixel, 0)).rgb;
				specular /= (Luminance(specular) + 1.f);

				specularSum       += specular * specularWeight;
				specularWeightSum += specularWeight;

				const float dwn = ComputeDiffuseNormalWeight(normal, sampleNormal);
				const float diffuseWeight = weight * dwn;

				float3 diffuse = u_diffuseTexture.Load(uint3(samplePixel, 0)).rgb;
				diffuse /= (Luminance(diffuse) + 1.f);

				diffuseSum       += diffuse *  diffuseWeight;
				diffuseWeightSum += diffuseWeight;
			}
		}

		float3 outSpecular = specularSum / specularWeightSum;
		outSpecular /= (1.f - Luminance(outSpecular));
		float3 outDiffuse  = diffuseSum / diffuseWeightSum;
		outDiffuse /= (1.f - Luminance(outDiffuse));

		u_rwSpecularTexture[pixel] = float4(outSpecular, centerSpecular.w);
		u_rwDiffuseTexture[pixel]  = float4(outDiffuse, centerDiffuse.w);
	}
}
