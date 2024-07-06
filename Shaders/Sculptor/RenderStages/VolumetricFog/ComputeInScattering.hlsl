#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(RenderSceneDS, 2)]]
[[descriptor_set(ViewShadingInputDS, 3)]]
[[descriptor_set(ShadowMapsDS, 4)]]
[[descriptor_set(ComputeInScatteringDS, 5)]]


#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Lighting.hlsli"
#include "Utils/Sampling.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};

[numthreads(4, 4, 4)]
void ComputeInScatteringCS(CS_INPUT input)
{
	if (all(input.globalID < u_fogConstants.fogGridRes))
	{
		const float projectionNearPlane = GetNearPlane(u_sceneView);

		const float fogNearPlane = u_fogConstants.fogNearPlane;
		const float fogFarPlane = u_fogConstants.fogFarPlane;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(u_fogConstants, u_sceneView, input.globalID.xyz, u_fogConstants.fogGridRes, u_depthTexture, u_depthSampler);

		const float4 scatteringExtinction = u_participatingMediaTexture.SampleLevel(u_nearestSample, fogFroxelUVW, 0);
 
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, projectionNearPlane);
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);
		
		const float froxelWDelta = u_fogConstants.fogGridInvRes.z;
		const float prevFogFroxelLinearDepth = ComputeFogFroxelLinearDepth(max(fogFroxelUVW.z - froxelWDelta, 0.f), fogNearPlane, fogFarPlane);

		InScatteringParams params;
		params.uv                       = fogFroxelUVW.xy;
		params.linearDepth              = fogFroxelLinearDepth;
		params.worldLocation            = fogFroxelWorldLocation;
		params.toViewNormal             = normalize(u_sceneView.viewLocation - fogFroxelWorldLocation);
		params.phaseFunctionAnisotrophy = u_inScatteringParams.localLightsPhaseFunctionAnisotrophy;
		params.inScatteringColor        = scatteringExtinction.rgb;
		params.froxelDepthRange         = fogFroxelLinearDepth - prevFogFroxelLinearDepth;
		
		float3 inScattering = ComputeLocalLightsInScattering(params);

		if(u_inScatteringParams.enableIndirectInScattering)
		{
			const float3 biasedIndirectUVW = fogFroxelUVW - float3(0.f, 0.f, froxelWDelta);
			const float3 indirectInScattering = u_indirectInScatteringTexture.SampleLevel(u_linearSampler, biasedIndirectUVW, 0.f);
			inScattering += indirectInScattering * scatteringExtinction.rgb;
		}

		float4 inScatteringExtinction = float4(inScattering, scatteringExtinction.w);

		if (u_inScatteringParams.hasValidHistory)
		{
			const float fogFroxelDepthNoJitter = fogFroxelUVW.z;
			const float fogFroxelLinearDepthNoJitter = ComputeFogFroxelLinearDepth(fogFroxelDepthNoJitter, fogNearPlane, fogFarPlane);

			const float3 fogFroxelNDCNoJitter = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepthNoJitter, projectionNearPlane);
			const float3 fogFroxelWorldLocationNoJitter = NDCToWorldSpaceNoJitter(fogFroxelNDCNoJitter, u_sceneView);

			float4 prevFrameClipSpace = mul(u_prevFrameSceneView.viewProjectionMatrixNoJitter, float4(fogFroxelWorldLocationNoJitter, 1.0f));
			
			if (all(prevFrameClipSpace.xy >= -prevFrameClipSpace.w) && all(prevFrameClipSpace.xyz <= prevFrameClipSpace.w) && prevFrameClipSpace.z >= 0.f)
			{
				prevFrameClipSpace.xyz /= prevFrameClipSpace.w;
				const float prevFrameLinearDepth = ComputeLinearDepth(prevFrameClipSpace.z, u_prevFrameSceneView);

				const float3 prevFrameFogFroxelUVW = ComputeFogFroxelUVW(prevFrameClipSpace.xy * 0.5f + 0.5f, prevFrameLinearDepth, fogNearPlane, fogFarPlane);

				float4 inScatteringExtinctionHistory = u_inScatteringHistoryTexture.SampleLevel(u_linearSampler, prevFrameFogFroxelUVW, 0);
				inScatteringExtinctionHistory.rgb = HistoryExposedLuminanceToLuminance(inScatteringExtinctionHistory.rgb);

				if (inScatteringExtinctionHistory.w > 0.f)
				{
					inScatteringExtinction = lerp(inScatteringExtinctionHistory, inScatteringExtinction, u_inScatteringParams.accumulationCurrentFrameWeight);
				}
			}
		}

		inScatteringExtinction.rgb = LuminanceToExposedLuminance(inScatteringExtinction.rgb);

		u_inScatteringTexture[input.globalID] = inScatteringExtinction;
	}
}
