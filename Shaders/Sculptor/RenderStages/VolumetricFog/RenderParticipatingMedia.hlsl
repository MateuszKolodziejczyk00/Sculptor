#include "SculptorShader.hlsli"


[[descriptor_set(RenderVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(RenderSceneDS, 2)]]
[[descriptor_set(RenderParticipatingMediaDS, 3)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/PerlinNoise.hlsli"
#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float ComputeDensityNoise(in float3 location)
{
	return perlin_noise::Evaluate(location) * 0.5f + 0.5f;
}


float EvaluateDensityAtLocation(in RenderParticipatingMediaParams params, in float3 location)
{
	const float globalDensity = params.constantFogDensity;
	return EvaluateHeightBasedDensityAtLocation(globalDensity, location, params.fogHeightFalloff);
}


[numthreads(4, 4, 4)]
void ParticipatingMediaCS(CS_INPUT input)
{
	if (all(input.globalID < u_fogConstants.fogGridRes))
	{
		float4 scatteringExtinction = 0.f;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(u_fogConstants, u_sceneView, input.globalID.xyz, u_fogConstants.fogGridRes, u_depthTexture, u_depthSampler);
		
		const float fogNearPlane = u_fogConstants.fogNearPlane;
		const float fogFarPlane = u_fogConstants.fogFarPlane;
		
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, GetNearPlane(u_sceneView));
		
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);

		const float density = EvaluateDensityAtLocation(u_participatingMediaParams, fogFroxelWorldLocation);

		// Apply constant fog term
		scatteringExtinction += ComputeScatteringAndExtinction(u_participatingMediaParams.constantFogAlbedo, u_participatingMediaParams.constantFogExtinction, density);

		u_participatingMediaTexture[input.globalID] = scatteringExtinction;
	}
}
