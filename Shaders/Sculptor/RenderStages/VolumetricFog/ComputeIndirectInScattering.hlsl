#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(IndirectInScatteringDS, 2)]]
[[descriptor_set(DDGISceneDS, 3)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "DDGI/DDGITypes.hlsli"
#include "Utils/SceneViewUtils.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 2)]
void ComputeIndirectInScatteringCS(CS_INPUT input)
{
	if (all(input.globalID < u_indirectInScatteringConstants.indirectGridRes))
	{
		const float projectionNearPlane = GetNearPlane(u_sceneView);

		const float fogNearPlane = u_fogConstants.fogNearPlane;
		const float fogFarPlane = u_fogConstants.fogFarPlane;

		const float bias = 0.2f;
		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(u_fogConstants, u_sceneView, input.globalID.xyz, u_indirectInScatteringConstants.indirectGridRes, u_depthTexture, u_depthSampler, bias);

		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, projectionNearPlane);

		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);
		
		DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(fogFroxelWorldLocation, 0.f, 0.f);
		ddgiSampleParams.sampleLocationBiasMultiplier = 0.f;
		const float3 indirect = DDGISampleAverageLuminance(ddgiSampleParams, DDGISampleContext::Create());
		const float phaseFunction = 1.f / (4.f * PI); // no anisotrophy

		const float3 inScattering = indirect * phaseFunction;

		const float integrationDomain = 4.f * PI;

		u_inScatteringTexture[input.globalID] = inScattering * integrationDomain;
	}
}
