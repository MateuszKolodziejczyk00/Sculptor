#include "SculptorShader.hlsli"

[[shader_params(VolumetricFogConstants, PARAMS_RENDER_VOLUMETRIC_FOG)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(IndirectInScatteringConstants, PARAMS_INDIRECT_IN_SCATTERING)]]
[[shader_params(DDGIGPUScene, PARAMS_D_D_G_I_SCENE)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "DDGI/DDGITypes.hlsli"
#include "Utils/SceneViewUtils.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 4)]
void ComputeIndirectInScatteringCS(CS_INPUT input)
{
	if (all(input.globalID < PARAMS_INDIRECT_IN_SCATTERING->indirectGridRes))
	{
		const float projectionNearPlane = GetNearPlane(VIEW->sceneView);

		const float fogNearPlane = PARAMS_RENDER_VOLUMETRIC_FOG->fogNearPlane;
		const float fogFarPlane = PARAMS_RENDER_VOLUMETRIC_FOG->fogFarPlane;

		const float bias = 0.2f;
		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(*PARAMS_RENDER_VOLUMETRIC_FOG, VIEW->sceneView, input.globalID.xyz, PARAMS_INDIRECT_IN_SCATTERING->indirectGridRes, PARAMS_RENDER_VOLUMETRIC_FOG->depthTexture, BindlessSamplers::LinearMinClampEdge(), bias);

		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, projectionNearPlane);

		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, VIEW->sceneView);
		
		DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(fogFroxelWorldLocation, 0.f, 0.f);
		ddgiSampleParams.sampleLocationBiasMultiplier = 0.f;
		const float3 indirect = DDGISampleAverageLuminance(ddgiSampleParams, DDGISampleContext::Create());
		const float phaseFunction = 1.f / (4.f * PI); // no anisotrophy

		const float3 inScattering = indirect * phaseFunction;

		const float integrationDomain = 4.f * PI;

		PARAMS_INDIRECT_IN_SCATTERING->inScatteringTexture[input.globalID] = inScattering * integrationDomain;
	}
}
