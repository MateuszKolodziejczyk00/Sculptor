#include "SculptorShader.hlsli"

[[shader_params(VolumetricFogConstants, PARAMS_RENDER_VOLUMETRIC_FOG)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(ViewShadingParams, SHADING)]]
[[shader_params(VolumetricFogInScatteringParams, CONSTS)]]


#define VOLUMETRIC_FOG_LIGHTING 1


#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Lighting.hlsli"
#include "Utils/Sampling.hlsli"
#include "Utils/Culling.hlsli"

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

		const float4 scatteringExtinction = CONSTS->participatingMediaTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), fogFroxelUVW, 0);
 
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
		inScatteringParams.phaseFunctionAnisotrophy   = CONSTS->paseFunctionAnisotrophy;
		inScatteringParams.inScatteringColor          = scatteringExtinction.rgb;
		inScatteringParams.froxelDepthRange           = fogFroxelLinearDepth - prevFogFroxelLinearDepth;
		inScatteringParams.directionalLightShadowTerm = CONSTS->directionalLightShadowTerm.Load(uint4(input.globalID, 0u)).r;

		float3 localLightsInScattering = 0.f;

		// Local lights in-scattering is visible only through composite, which samples integrated fog at the surface depth with tricubic filter (2 froxels footprint),
		// so froxels behind the furthest surface of that footprint can be skipped. One more froxel of margin covers depth shift in ComputeFogGridSampleUVW
		const float4 footprintUV = float4(float2(input.globalID.xy) - 2.f, float2(input.globalID.xy) + 3.f) * PARAMS_RENDER_VOLUMETRIC_FOG->fogGridInvRes.xyxy;
		const float footprintFurthestDepth = SampleHiZFurthestDepth(CONSTS->hiZTexture, BindlessSamplers::LinearMinClampEdge(), CONSTS->hiZResolution, footprintUV);
		const float footprintFurthestW = footprintFurthestDepth > 0.f ? saturate(ComputeFogFroxelUVW(0.f, ComputeLinearDepth(footprintFurthestDepth, VIEW->sceneView), fogNearPlane, fogFarPlane).z) : 1.f;
		const uint lastVisibleFroxelZ = uint(footprintFurthestW * PARAMS_RENDER_VOLUMETRIC_FOG->fogGridRes.z) + 3u;

		const bool isWithinLocalLightsScatteringRange = input.globalID.z <= lastVisibleFroxelZ && input.globalID.z < PARAMS_RENDER_VOLUMETRIC_FOG->localLightsScatteringMaxDepth;

		if (isWithinLocalLightsScatteringRange)
		{
			localLightsInScattering = ComputeLocalLightsInScattering(inScatteringParams);
		}
		
		float3 inScattering = ComputeDirectionalLightsInScattering(inScatteringParams) + localLightsInScattering;

		if(CONSTS->enableIndirectInScattering)
		{
			const float3 biasedIndirectUVW = fogFroxelUVW - float3(0.f, 0.f, froxelWDelta);
			const float3 indirectInScattering = CONSTS->indirectInScatteringTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), biasedIndirectUVW, 0.f);
			inScattering += indirectInScattering * scatteringExtinction.rgb;
		}

		float4 inScatteringExtinction = float4(inScattering, scatteringExtinction.w);

		inScatteringExtinction.rgb = LuminanceToExposedLuminance(inScatteringExtinction.rgb);

		CONSTS->inScatteringTexture[input.globalID] = inScatteringExtinction;
	}
}
