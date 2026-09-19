#include "SculptorShader.hlsli"

[[shader_params(VolumetricFogConstants, PARAMS_RENDER_VOLUMETRIC_FOG)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(ViewShadingParams, PARAMS_VIEW_SHADING_INPUT)]]
[[shader_params(VolumetricFogShadowTermConstants, PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM)]]


#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Shadows.hlsli"
#include "SceneRendering/WSC.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float ComputeShadowTerm(in float3 worldLocation, in float3 toView, in float3 froxelDepthRange)
{
	// Directional Lights

	SPT_CHECK_MSG(PARAMS_VIEW_SHADING_INPUT->lightsData->directionalLightsNum <= 1u, L"Volumetric Fog supports shadow term only for 1 directional light!");

	if(PARAMS_VIEW_SHADING_INPUT->lightsData->directionalLightsNum == 0u)
	{
		return 0.f;
	}

	const DirectionalLightGPUData directionalLight = PARAMS_VIEW_SHADING_INPUT->directionalLights[0];

	float visibility = WSC().SampleShadows(worldLocation);

	const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(PARAMS_VIEW_SHADING_INPUT->lightsData->heightFog, worldLocation, worldLocation - directionalLight.direction * 1500.f);
	visibility *= fogTransmittance;

	return visibility;
}


[numthreads(4, 4, 4)]
void ComputeDirectionalLightShadowTermCS(CS_INPUT input)
{
	if (all(input.globalID < PARAMS_RENDER_VOLUMETRIC_FOG->fogGridRes))
	{
		const float projectionNearPlane = GetNearPlane(VIEW->sceneView);

		const float fogNearPlane = PARAMS_RENDER_VOLUMETRIC_FOG->fogNearPlane;
		const float fogFarPlane = PARAMS_RENDER_VOLUMETRIC_FOG->fogFarPlane;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(*PARAMS_RENDER_VOLUMETRIC_FOG, VIEW->sceneView, input.globalID.xyz, PARAMS_RENDER_VOLUMETRIC_FOG->fogGridRes, PARAMS_RENDER_VOLUMETRIC_FOG->depthTexture, BindlessSamplers::LinearMinClampEdge());
 
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, projectionNearPlane);
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, VIEW->sceneView);
		
		const float froxelWDelta = PARAMS_RENDER_VOLUMETRIC_FOG->fogGridInvRes.z;
		const float prevFogFroxelLinearDepth = ComputeFogFroxelLinearDepth(max(fogFroxelUVW.z - froxelWDelta, 0.f), fogNearPlane, fogFarPlane);

		const float3 toViewNormal = normalize(VIEW->sceneView.viewLocation - fogFroxelWorldLocation);
		const float froxelDepthRange = fogFroxelLinearDepth - prevFogFroxelLinearDepth;

		float shadowTerm = ComputeShadowTerm(fogFroxelWorldLocation, toViewNormal, froxelDepthRange);

		if(PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->hasCloudsTransmittanceMap)
		{
			const float4 ctmCS = mul(PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->cloudsTransmittanceMapViewProj, float4(fogFroxelWorldLocation, 1.f));
			if(all(ctmCS.xy <= ctmCS.w) && all(ctmCS.xy >= -ctmCS.w))
			{
				const float2 ctmUV = (ctmCS.xy / ctmCS.w) *	0.5f + 0.5f;

				shadowTerm *= PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->cloudsTransmittanceMap.SampleLevel(BindlessSamplers::LinearClampEdge(), ctmUV, 0.f);
			}
		}

		if (PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->hasValidHistory)
		{
			const float fogFroxelDepthNoJitter = fogFroxelUVW.z;
			const float fogFroxelLinearDepthNoJitter = ComputeFogFroxelLinearDepth(fogFroxelDepthNoJitter, fogNearPlane, fogFarPlane);

			const float3 fogFroxelNDCNoJitter = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepthNoJitter, projectionNearPlane);
			const float3 fogFroxelWorldLocationNoJitter = NDCToWorldSpaceNoJitter(fogFroxelNDCNoJitter, VIEW->sceneView);

			float4 prevFrameClipSpace = mul(VIEW->prevFrameSceneView.viewProjectionMatrixNoJitter, float4(fogFroxelWorldLocationNoJitter, 1.0f));
			
			if (all(prevFrameClipSpace.xy >= -prevFrameClipSpace.w) && all(prevFrameClipSpace.xyz <= prevFrameClipSpace.w) && prevFrameClipSpace.z >= 0.f)
			{
				prevFrameClipSpace.xyz /= prevFrameClipSpace.w;
				const float prevFrameLinearDepth = ComputeLinearDepth(prevFrameClipSpace.z, VIEW->prevFrameSceneView);

				const float3 prevFrameFogFroxelUVW = ComputeFogFroxelUVW(prevFrameClipSpace.xy * 0.5f + 0.5f, prevFrameLinearDepth, fogNearPlane, fogFarPlane);

				const float historyShadowTerm = PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->historyDirLightShadowTerm.SampleLevel(BindlessSamplers::LinearClampEdge(), prevFrameFogFroxelUVW, 0);

				shadowTerm = lerp(historyShadowTerm, shadowTerm, PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->accumulationCurrentFrameWeight);
			}
		}

		PARAMS_COMPUTE_DIRECTIONAL_LIGHT_SHADOW_TERM->rwDirLightShadowTerm[input.globalID] = shadowTerm;
	}
}
