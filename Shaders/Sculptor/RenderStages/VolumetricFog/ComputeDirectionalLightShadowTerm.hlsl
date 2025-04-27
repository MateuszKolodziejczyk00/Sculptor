#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(RenderSceneDS, 2)]]
[[descriptor_set(ViewShadingInputDS, 3)]]
[[descriptor_set(ShadowMapsDS, 4)]]
[[descriptor_set(ComputeDirectionalLightShadowTermDS, 5)]]


#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Shadows.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float ComputeShadowTerm(in float3 worldLocation, in float3 toView, in float3 froxelDepthRange)
{
	// Directional Lights

	SPT_CHECK_MSG(u_lightsData.directionalLightsNum <= 1u, L"Volumetric Fog supports shadow term only for 1 directional light!");

	if(u_lightsData.directionalLightsNum == 0u)
	{
		return 0.f;
	}

	const DirectionalLightGPUData directionalLight = u_directionalLights[0];

	float visibility = 1.f;
	if(directionalLight.shadowCascadesNum != 0)
	{
		const float3 beginSampleLocation = worldLocation + toView * (froxelDepthRange * 0.5f);
		const float3 endSampleLocation   = worldLocation - toView * (froxelDepthRange * 0.5f);
		const uint samplesNum = 2u;
		visibility = EvaluateCascadedShadowsAtLine(beginSampleLocation, endSampleLocation, samplesNum, directionalLight.firstShadowCascadeIdx, directionalLight.shadowCascadesNum);

	}

	return visibility;
}


[numthreads(4, 4, 4)]
void ComputeDirectionalLightShadowTermCS(CS_INPUT input)
{
	if (all(input.globalID < u_fogConstants.fogGridRes))
	{
		const float projectionNearPlane = GetNearPlane(u_sceneView);

		const float fogNearPlane = u_fogConstants.fogNearPlane;
		const float fogFarPlane = u_fogConstants.fogFarPlane;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(u_fogConstants, u_sceneView, input.globalID.xyz, u_fogConstants.fogGridRes, u_depthTexture, u_depthSampler);
 
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, projectionNearPlane);
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);
		
		const float froxelWDelta = u_fogConstants.fogGridInvRes.z;
		const float prevFogFroxelLinearDepth = ComputeFogFroxelLinearDepth(max(fogFroxelUVW.z - froxelWDelta, 0.f), fogNearPlane, fogFarPlane);

		const float3 toViewNormal = normalize(u_sceneView.viewLocation - fogFroxelWorldLocation);
		const float froxelDepthRange = fogFroxelLinearDepth - prevFogFroxelLinearDepth;

		float shadowTerm = ComputeShadowTerm(fogFroxelWorldLocation, toViewNormal, froxelDepthRange);

		if (u_constants.hasValidHistory)
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

				const float historyShadowTerm = u_historyDirLightShadowTerm.SampleLevel(u_linearSampler, prevFrameFogFroxelUVW, 0);

				shadowTerm = lerp(historyShadowTerm, shadowTerm, u_constants.accumulationCurrentFrameWeight);
			}
		}

		u_rwDirLightShadowTerm[input.globalID] = shadowTerm;
	}
}
