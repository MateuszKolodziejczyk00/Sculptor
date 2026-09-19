#include "SculptorShader.hlsli"

[[shader_params(VolumetricFogConstants, PARAMS_RENDER_VOLUMETRIC_FOG)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(ViewShadingParams, PARAMS_VIEW_SHADING_INPUT)]]
[[shader_params(VolumetricFogInScatteringParams, PARAMS_COMPUTE_IN_SCATTERING)]]


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
	if (all(input.globalID < PARAMS_RENDER_VOLUMETRIC_FOG->fogGridRes))
	{
		const float projectionNearPlane = GetNearPlane(VIEW->sceneView);

		const float fogNearPlane = PARAMS_RENDER_VOLUMETRIC_FOG->fogNearPlane;
		const float fogFarPlane = PARAMS_RENDER_VOLUMETRIC_FOG->fogFarPlane;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(*PARAMS_RENDER_VOLUMETRIC_FOG, VIEW->sceneView, input.globalID.xyz, PARAMS_RENDER_VOLUMETRIC_FOG->fogGridRes, PARAMS_RENDER_VOLUMETRIC_FOG->depthTexture, BindlessSamplers::LinearMinClampEdge());

		const float4 scatteringExtinction = PARAMS_COMPUTE_IN_SCATTERING->participatingMediaTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), fogFroxelUVW, 0);
 
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, projectionNearPlane);
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, VIEW->sceneView);
		
		const float froxelWDelta = PARAMS_RENDER_VOLUMETRIC_FOG->fogGridInvRes.z;
		const float prevFogFroxelLinearDepth = ComputeFogFroxelLinearDepth(max(fogFroxelUVW.z - froxelWDelta, 0.f), fogNearPlane, fogFarPlane);

		InScatteringParams inScatteringParams;
		inScatteringParams.uv                         = fogFroxelUVW.xy;
		inScatteringParams.linearDepth                = fogFroxelLinearDepth;
		inScatteringParams.worldLocation              = fogFroxelWorldLocation;
		inScatteringParams.toViewNormal               = normalize(VIEW->sceneView.viewLocation - fogFroxelWorldLocation);
		inScatteringParams.phaseFunctionAnisotrophy   = PARAMS_COMPUTE_IN_SCATTERING->paseFunctionAnisotrophy;
		inScatteringParams.inScatteringColor          = scatteringExtinction.rgb;
		inScatteringParams.froxelDepthRange           = fogFroxelLinearDepth - prevFogFroxelLinearDepth;
		inScatteringParams.directionalLightShadowTerm = PARAMS_COMPUTE_IN_SCATTERING->directionalLightShadowTerm.Load(uint4(input.globalID, 0u)).r;

		float3 localLightsInScattering = 0.f;

		const bool isWithinLocalLightsScatteringRange = input.globalID.z < PARAMS_RENDER_VOLUMETRIC_FOG->localLightsScatteringMaxDepth;

		if (isWithinLocalLightsScatteringRange)
		{
			localLightsInScattering = ComputeLocalLightsInScattering(inScatteringParams);

			if(PARAMS_COMPUTE_IN_SCATTERING->hasValidHistory)
			{
				const float3 historyNDC = WorldSpaceToNDCNoJitter(fogFroxelWorldLocation, VIEW->prevFrameSceneView);
				const float historyLinearDepth = ComputeLinearDepth(historyNDC.z, VIEW->prevFrameSceneView);
				float3 historyUVW = ComputeFogFroxelUVW(historyNDC.xy * 0.5f + 0.5f, historyLinearDepth, fogNearPlane, fogFarPlane);
				historyUVW.z *= PARAMS_RENDER_VOLUMETRIC_FOG->localLightsScatteringWNormalization;

				if(all(abs(historyNDC.xy) < 1.f) && historyNDC.z > 0.f && historyNDC.z < 1.f)
				{
					const float3 localLightsInScatteringHistory = PARAMS_COMPUTE_IN_SCATTERING->historyLocalLightsInScatteringTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), historyUVW, 0.f).xyz;

					localLightsInScattering = lerp(localLightsInScatteringHistory, localLightsInScattering, 0.25f);
				}
			}
		}
		
		float3 inScattering = ComputeDirectionalLightsInScattering(inScatteringParams) + localLightsInScattering;

		if(PARAMS_COMPUTE_IN_SCATTERING->enableIndirectInScattering)
		{
			const float3 biasedIndirectUVW = fogFroxelUVW - float3(0.f, 0.f, froxelWDelta);
			const float3 indirectInScattering = PARAMS_COMPUTE_IN_SCATTERING->indirectInScatteringTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), biasedIndirectUVW, 0.f);
			inScattering += indirectInScattering * scatteringExtinction.rgb;
		}

		float4 inScatteringExtinction = float4(inScattering, scatteringExtinction.w);

		inScatteringExtinction.rgb = LuminanceToExposedLuminance(inScatteringExtinction.rgb);

		PARAMS_COMPUTE_IN_SCATTERING->inScatteringTexture[input.globalID] = inScatteringExtinction;

		if (isWithinLocalLightsScatteringRange)
		{
			PARAMS_COMPUTE_IN_SCATTERING->localLightsInScatteringTexture[input.globalID] = float4(localLightsInScattering, 0.f);
		}
	}
}
