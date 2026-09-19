#include "SculptorShader.hlsli"

[[shader_params(SRDisocclusionFixConstants, PARAMS_S_R_DISOCCLUSION_FIX)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/Denoiser/RTDenoising.hlsli"
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

	if(all(pixel < PARAMS_S_R_DISOCCLUSION_FIX->resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * PARAMS_S_R_DISOCCLUSION_FIX->pixelSize;

		const float3 normal = OctahedronDecodeNormal(PARAMS_S_R_DISOCCLUSION_FIX->normalsTexture.Load(uint3(pixel, 0)));

		const float kernel[3] = { 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };

		const float centerDepth = PARAMS_S_R_DISOCCLUSION_FIX->depthTexture.Load(uint3(pixel, 0));

		const float roughness = PARAMS_S_R_DISOCCLUSION_FIX->roughnessTexture.Load(uint3(pixel, 0));

		const uint specularHistoryLength =	PARAMS_S_R_DISOCCLUSION_FIX->specularHistoryLengthTexture.Load(int3(pixel, 0));
		const uint diffuseHistoryLength  = PARAMS_S_R_DISOCCLUSION_FIX->diffuseHistoryLengthTexture  .Load(int3(pixel, 0));

		const bool fixSpecular = specularHistoryLength < 3;
		const bool fixDiffuse  = diffuseHistoryLength < 5;

		if((!fixSpecular && !fixDiffuse) || roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			PARAMS_S_R_DISOCCLUSION_FIX->outSpecularY_SH2[pixel]  = PARAMS_S_R_DISOCCLUSION_FIX->inSpecularY_SH2.Load(uint3(pixel, 0));
			PARAMS_S_R_DISOCCLUSION_FIX->outDiffuseY_SH2[pixel]   = PARAMS_S_R_DISOCCLUSION_FIX->inDiffuseY_SH2.Load(uint3(pixel, 0));
			PARAMS_S_R_DISOCCLUSION_FIX->outDiffSpecCoCg[pixel]   = PARAMS_S_R_DISOCCLUSION_FIX->inDiffSpecCoCg.Load(uint3(pixel, 0));
			return;
		}
		
		const float3 centerWS = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, centerDepth), VIEW->sceneView);

		const float centerWeight = kernel[0];

		const RTSphericalBasis centerSpecularY_SH2 = RawToRTSphericalBasis(PARAMS_S_R_DISOCCLUSION_FIX->inSpecularY_SH2.Load(uint3(pixel, 0)));
		const RTSphericalBasis centerDiffuseY_SH2  = RawToRTSphericalBasis(PARAMS_S_R_DISOCCLUSION_FIX->inDiffuseY_SH2.Load(uint3(pixel, 0)));
		const float4 centerDiffSpecCoCg      = PARAMS_S_R_DISOCCLUSION_FIX->inDiffSpecCoCg.Load(uint3(pixel, 0));

		RTSphericalBasis specularY_SH2Sum = centerSpecularY_SH2 * centerWeight;
		RTSphericalBasis diffuseY_SH2Sum  = centerDiffuseY_SH2 * centerWeight;
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

				const int2 samplePixel = clamp(pixel + int2(x, y) * PARAMS_S_R_DISOCCLUSION_FIX->filterStride, int2(0, 0), int2(PARAMS_S_R_DISOCCLUSION_FIX->resolution - 1));
				const float w = kernel[max(abs(x), abs(y))];

				const float3 sampleNormal = OctahedronDecodeNormal(PARAMS_S_R_DISOCCLUSION_FIX->normalsTexture.Load(uint3(samplePixel, 0)));

				const float sampleDepth = PARAMS_S_R_DISOCCLUSION_FIX->depthTexture.Load(uint3(samplePixel, 0));

				if(sampleDepth < 0.000001f)
				{
					continue;
				}

				const float3 sampleWS = NDCToWorldSpaceNoJitter(float3((uv + float2(x, y) * PARAMS_S_R_DISOCCLUSION_FIX->pixelSize) * 2.f - 1.f, sampleDepth), VIEW->sceneView);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);

				const float weight = dw * w;

				const float4 sampleDiffSpecCoCg = PARAMS_S_R_DISOCCLUSION_FIX->inDiffSpecCoCg.Load(uint3(samplePixel, 0));

				if(fixSpecular)
				{
					const float swn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
					float specularWeight = weight * swn;

					const RTSphericalBasis sampleSpecularY_SH2 = RawToRTSphericalBasis(PARAMS_S_R_DISOCCLUSION_FIX->inSpecularY_SH2.Load(uint3(samplePixel, 0)));

					const float specularLum = sampleSpecularY_SH2.Evaluate(sampleNormal);

					specularY_SH2Sum = specularY_SH2Sum + sampleSpecularY_SH2 * specularWeight;
					diffSpecCoCgSum.zw += sampleDiffSpecCoCg.zw * specularWeight;
					specularWeightSum += specularWeight;
				}

				if(fixDiffuse)
				{
					const float dwn = ComputeDiffuseNormalWeight(normal, sampleNormal);
					float diffuseWeight = weight * dwn;

					const RTSphericalBasis sampleDiffuseY_SH2 = RawToRTSphericalBasis(PARAMS_S_R_DISOCCLUSION_FIX->inDiffuseY_SH2.Load(uint3(samplePixel, 0)));

					const float diffuseLum = sampleDiffuseY_SH2.Evaluate(sampleNormal);

					diffuseY_SH2Sum = diffuseY_SH2Sum + sampleDiffuseY_SH2 * diffuseWeight;
					diffSpecCoCgSum.xy += sampleDiffSpecCoCg.xy * diffuseWeight;
					diffuseWeightSum += diffuseWeight;
				}
			}
		}

		const float rcpSpecularWeightSum = 1.f / specularWeightSum;
		const float rcpDiffuseWeightSum = 1.f / diffuseWeightSum;

		const RTSphericalBasis outSpecularY_SH2 = specularY_SH2Sum * rcpSpecularWeightSum;
		const RTSphericalBasis outDiffuseY_SH2 = diffuseY_SH2Sum * rcpDiffuseWeightSum;
		const float4 outDiffSpecCoCg = diffSpecCoCgSum * float4(rcpDiffuseWeightSum, rcpDiffuseWeightSum, rcpSpecularWeightSum, rcpSpecularWeightSum);

		PARAMS_S_R_DISOCCLUSION_FIX->outSpecularY_SH2[pixel]  = RTSphericalBasisToRaw(outSpecularY_SH2);
		PARAMS_S_R_DISOCCLUSION_FIX->outDiffuseY_SH2[pixel]   = RTSphericalBasisToRaw(outDiffuseY_SH2);
		PARAMS_S_R_DISOCCLUSION_FIX->outDiffSpecCoCg[pixel]   = outDiffSpecCoCg;
	}
}
