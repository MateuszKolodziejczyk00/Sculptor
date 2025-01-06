#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRTemporalAccumulationDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


#define HISTORY_SAMPLE_COUNT 5

static const int2 g_historyOffsets[HISTORY_SAMPLE_COUNT] =
{
	int2(0, 0),
	int2(-1, 0),
	int2(1, 0),
	int2(0, 1),
	int2(0, -1)
};


void FindTemporalReprojection(in float2 surfaceHistoryUV, 
                              in float3 currentSampleNormal,
                              in float3 currentSampleWS,
                              in float2 pixelSize,
                              in float maxPlaneDistance,
                              in float roughness,
                              out float2 reprojectedUV,
                              out float reprojectionConfidence)
{
	reprojectedUV = surfaceHistoryUV;

	const float maxRoguhnessDiff = roughness * 0.1f;

	reprojectionConfidence = 0.f;

	const Plane currentSamplePlane = Plane::Create(currentSampleNormal, currentSampleWS);

	if (all(reprojectedUV >= 0.f) && all(reprojectedUV <= 1.f))
	{
		const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, reprojectedUV, 0.f);

		float2 bestHistoryUV = reprojectedUV;

		for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
		{
			const float2 sampleUV = reprojectedUV + g_historyOffsets[sampleIdx] * pixelSize;

			const float3 historySampleNDC = float3(sampleUV * 2.f - 1.f, historySampleDepth);
			const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

			const float historySampleDistToCurrentSamplePlane = currentSamplePlane.Distance(historySampleWS);
			if (abs(historySampleDistToCurrentSamplePlane) < maxPlaneDistance)
			{
				const float3 historyNormals  = OctahedronDecodeNormal(u_historyNormalsTexture.SampleLevel(u_linearSampler, sampleUV, 0.f));
				const float historyRoughness = u_historyRoughnessTexture.SampleLevel(u_linearSampler, sampleUV, 0.f);

				const float normalsSimilarity = dot(historyNormals, currentSampleNormal);
				if(normalsSimilarity >= 0.9f && abs(roughness - historyRoughness) < maxRoguhnessDiff)
				{
					const float3 historyViewDir = normalize(u_prevFrameSceneView.viewLocation - historySampleWS);
					const float3 currentViewDir = normalize(u_sceneView.viewLocation - currentSampleWS);
					const float parallaxAngle = dot(historyViewDir, currentViewDir);

					const bool isCenterSample = sampleIdx == 0;

					const float sampleReprojectionConfidence = saturate(1.f - 0.3f * saturate(1.f - parallaxAngle) + (isCenterSample ? 0.04f : 0.f));

					if(sampleReprojectionConfidence > reprojectionConfidence)
					{
						reprojectionConfidence = sampleReprojectionConfidence;
						bestHistoryUV = sampleUV;

						if(reprojectionConfidence > 0.99f)
						{
							break;
						}
					}
				}
			}
		}

		reprojectedUV = bestHistoryUV;
	}
}

[numthreads(8, 8, 1)]
void SRTemporalAccumulationCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_rwSpecularTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float4 specular = u_rwSpecularTexture[pixel];
		const float4 diffuse  = u_rwDiffuseTexture[pixel];

		const float currentDepth = u_depthTexture.Load(uint3(pixel, 0));
		if(currentDepth == 0.f)
		{
			return;
		}

		const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
		const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, u_sceneView);
		const float3 projectedReflectionWS = currentSampleWS + normalize(currentSampleWS - u_sceneView.viewLocation) * specular.w;
		const float3 prevFrameNDC = WorldSpaceToNDCNoJitter(projectedReflectionWS, u_prevFrameSceneView);

		const float currentLinearDepth = ComputeLinearDepth(currentDepth, u_sceneView);
		const float maxPlaneDistance = max(0.015f * currentLinearDepth, 0.025f);

		const float2 virtualPointHistoryUV = prevFrameNDC.xy * 0.5f + 0.5f;

		const float2 motion = u_motionTexture.Load(uint3(pixel, 0));
		const float2 surfaceHistoryUV = uv - motion;

		const float3 currentSampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		float2 diffuseReprojectedUV;
		float diffuseReprojectionConfidence;
		FindTemporalReprojection(surfaceHistoryUV,
		                         currentSampleNormal,
		                         currentSampleWS,
		                         pixelSize,
		                         maxPlaneDistance,
		                         roughness,
		                         OUT diffuseReprojectedUV,
		                         OUT diffuseReprojectionConfidence);

		const float diffuseLum = Luminance(diffuse.rgb);
		float2 diffuseMoments = float2(diffuseLum, Pow2(diffuseLum));
		uint diffuseHistoryLength = 1u;

		if (diffuseReprojectionConfidence > 0.05f)
		{
			const uint3 historyPixel = uint3(diffuseReprojectedUV * outputRes.xy, 0u);

			diffuseHistoryLength += u_historyDiffuseHistoryLengthTexture.Load(historyPixel);

			diffuseHistoryLength = min(diffuseHistoryLength, uint(diffuseReprojectionConfidence * MAX_ACCUMULATED_FRAMES_NUM));
			float currentFrameWeight = rcp(float(diffuseHistoryLength + 1u));

			const float2 diffuseHistoryMoments = u_diffuseHistoryTemporalVarianceTexture.Load(historyPixel);
			diffuseMoments = lerp(diffuseHistoryMoments, diffuseMoments, currentFrameWeight);

			const float diffuseFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - diffuseReprojectionConfidence);

			float4 diffuseHistory = u_diffuseHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			diffuseHistory.rgb = ExposedHistoryLuminanceToCurrentExposedLuminance(diffuseHistory.rgb);

			const float4 accumulatedDiffuse  = lerp(diffuseHistory, diffuse, diffuseFrameWeight);

			u_rwDiffuseTexture[pixel]  = accumulatedDiffuse;

			// Fast history

			const float currentFrameWeightFast = max(0.3f, diffuseFrameWeight);

			const float3 diffuseFastHistory = u_diffuseFastHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			u_rwDiffuseFastHistoryTexture[pixel] = lerp(diffuseFastHistory, diffuse.rgb, currentFrameWeightFast);
		}
		else
		{
			u_rwDiffuseFastHistoryTexture[pixel]  = diffuse.rgb;
		}

		u_diffuseHistoryLengthTexture[pixel] = diffuseHistoryLength;

		float2 specularReprojectedUV;
		float specularReprojectionConfidence;
		FindTemporalReprojection(virtualPointHistoryUV,
		                         currentSampleNormal,
		                         currentSampleWS,
		                         pixelSize,
		                         maxPlaneDistance,
		                         roughness,
		                         OUT specularReprojectedUV,
		                         OUT specularReprojectionConfidence);

		const float specularLum = Luminance(specular.rgb);
		float2 specularMoments = float2(specularLum, Pow2(specularLum));
		uint specularHistoryLength = 1u;

		if (specularReprojectionConfidence > 0.05f)
		{
			const uint3 historyPixel = uint3(specularReprojectedUV * outputRes.xy, 0u);

			specularHistoryLength += u_historySpecularHistoryLengthTexture.Load(historyPixel);

			specularHistoryLength = min(specularHistoryLength, uint(specularReprojectionConfidence * MAX_ACCUMULATED_FRAMES_NUM));
			float currentFrameWeight = rcp(float(specularHistoryLength + 1u));

			const float2 specularHistoryMoments = u_specularHistoryTemporalVarianceTexture.Load(historyPixel);
			specularMoments = lerp(specularHistoryMoments, specularMoments, currentFrameWeight);

			const float specularFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - specularReprojectionConfidence);
			
			float4 specularHistory = u_specularHistoryTexture.SampleLevel(u_linearSampler, specularReprojectedUV, 0.0f);
			specularHistory.rgb = ExposedHistoryLuminanceToCurrentExposedLuminance(specularHistory.rgb);

			const float4 accumulatedSpecular = lerp(specularHistory, specular, specularFrameWeight);

			u_rwSpecularTexture[pixel] = accumulatedSpecular;

			// Fast history

			const float currentFrameWeightFast = max(0.3f, specularFrameWeight);

			const float3 specularFastHistory = u_specularFastHistoryTexture.SampleLevel(u_linearSampler, specularReprojectedUV, 0.f);
			u_rwSpecularFastHistoryTexture[pixel] = lerp(specularFastHistory, specular.rgb, currentFrameWeightFast);
		}
		else
		{
			u_rwSpecularFastHistoryTexture[pixel] = specular.rgb;
		}

		u_specularHistoryLengthTexture[pixel] = specularHistoryLength;

		u_rwSpecularTemporalVarianceTexture[pixel] = specularMoments;
		u_rwDiffuseTemporalVarianceTexture[pixel]  = diffuseMoments;
	}
}
