#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricFogDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(ViewShadingInputDS)]]
[[descriptor_set(ComputeInScatteringDS)]]


#define VOLUMETRIC_FOG_LIGHTING 1


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

		InScatteringParams inScatteringParams;
		inScatteringParams.uv                         = fogFroxelUVW.xy;
		inScatteringParams.linearDepth                = fogFroxelLinearDepth;
		inScatteringParams.worldLocation              = fogFroxelWorldLocation;
		inScatteringParams.toViewNormal               = normalize(u_sceneView.viewLocation - fogFroxelWorldLocation);
		inScatteringParams.phaseFunctionAnisotrophy   = u_inScatteringParams.paseFunctionAnisotrophy;
		inScatteringParams.inScatteringColor          = scatteringExtinction.rgb;
		inScatteringParams.froxelDepthRange           = fogFroxelLinearDepth - prevFogFroxelLinearDepth;
		inScatteringParams.directionalLightShadowTerm = u_directionalLightShadowTerm.Load(uint4(input.globalID, 0u)).r;

		float3 localLightsInScattering = 0.f;

		const bool isWithinLocalLightsScatteringRange = input.globalID.z < u_fogConstants.localLightsScatteringMaxDepth;

		if (isWithinLocalLightsScatteringRange)
		{
			localLightsInScattering = ComputeLocalLightsInScattering(inScatteringParams);

			if(u_inScatteringParams.hasValidHistory)
			{
				const float3 historyNDC = WorldSpaceToNDCNoJitter(fogFroxelWorldLocation, u_prevFrameSceneView);
				const float historyLinearDepth = ComputeLinearDepth(historyNDC.z, u_prevFrameSceneView);
				float3 historyUVW = ComputeFogFroxelUVW(historyNDC.xy * 0.5f + 0.5f, historyLinearDepth, fogNearPlane, fogFarPlane);
				historyUVW.z *= u_fogConstants.localLightsScatteringWNormalization;

				if(all(abs(historyNDC.xy) < 1.f) && historyNDC.z > 0.f && historyNDC.z < 1.f)
				{
					const float3 localLightsInScatteringHistory = u_historyLocalLightsInScatteringTexture.SampleLevel(u_linearSampler, historyUVW, 0.f).xyz;

					localLightsInScattering = lerp(localLightsInScatteringHistory, localLightsInScattering, 0.25f);
				}
			}
		}
		
		float3 inScattering = ComputeDirectionalLightsInScattering(inScatteringParams) + localLightsInScattering;

		if(u_inScatteringParams.enableIndirectInScattering)
		{
			const float3 biasedIndirectUVW = fogFroxelUVW - float3(0.f, 0.f, froxelWDelta);
			const float3 indirectInScattering = u_indirectInScatteringTexture.SampleLevel(u_linearSampler, biasedIndirectUVW, 0.f);
			inScattering += indirectInScattering * scatteringExtinction.rgb;
		}

		float4 inScatteringExtinction = float4(inScattering, scatteringExtinction.w);

		inScatteringExtinction.rgb = LuminanceToExposedLuminance(inScatteringExtinction.rgb);

		u_inScatteringTexture[input.globalID]            = inScatteringExtinction;

		if (isWithinLocalLightsScatteringRange)
		{
			u_localLightsInScatteringTexture[input.globalID] = float4(localLightsInScattering, 0.f);
		}
	}
}
