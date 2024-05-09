#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(ViewShadingInputDS, 2)]]
[[descriptor_set(ShadowMapsDS, 3)]]
[[descriptor_set(ComputeInScatteringDS, 4)]]


#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Lighting.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 4)]
void ComputeInScatteringCS(CS_INPUT input)
{
	uint3 volumetricFogResolution;
	u_inScatteringTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

	if (all(input.globalID < volumetricFogResolution))
	{
		const float3 fogResolutionRcp = rcp(float3(volumetricFogResolution));
		const float3 fogFroxelUVW = (float3(input.globalID) + 0.5f) * fogResolutionRcp;

		const float projectionNearPlane = GetNearPlane(u_sceneView);

		const float fogNearPlane = u_inScatteringParams.fogNearPlane;
		const float fogFarPlane = u_inScatteringParams.fogFarPlane;

		const float4 scatteringExtinction = u_participatingMediaTexture.SampleLevel(u_participatingMediaSampler, fogFroxelUVW, 0);

		const float3 jitter = u_inScatteringParams.jitter;

		const float fogFroxelDepth = fogFroxelUVW.z + jitter.z;
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelDepth, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy + jitter.xy, fogFroxelLinearDepth, projectionNearPlane);

		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);
		
		InScatteringParams params;
		params.uv                       = fogFroxelUVW.xy;
		params.linearDepth              = fogFroxelLinearDepth;
		params.worldLocation            = fogFroxelWorldLocation;
		params.toViewNormal             = normalize(u_sceneView.viewLocation - fogFroxelWorldLocation);
		params.phaseFunctionAnisotrophy = u_inScatteringParams.localLightsPhaseFunctionAnisotrophy;
		params.inScatteringColor        = scatteringExtinction.rgb;
		
		float3 inScattering = ComputeLocalLightsInScattering(params);

		if(u_inScatteringParams.enableIndirectInScattering)
		{
			const float3 indirectInScattering = u_indirectInScatteringTexture.SampleLevel(u_indirectInScatteringSampler, fogFroxelUVW, 0.f);
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

				float4 inScatteringExtinctionHistory = u_inScatteringHistoryTexture.SampleLevel(u_inScatteringHistorySampler, prevFrameFogFroxelUVW, 0);
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
