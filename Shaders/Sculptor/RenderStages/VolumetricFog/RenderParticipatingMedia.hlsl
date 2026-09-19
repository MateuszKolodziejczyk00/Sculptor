#include "SculptorShader.hlsli"


[[shader_params(VolumetricFogConstants, FOG)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(RenderParticipatingMediaParams, CONSTS)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/PerlinNoise.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "SceneRendering/GPUScene.hlsli"


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
	if (all(input.globalID < FOG->fogGridRes))
	{
		float4 scatteringExtinction = 0.f;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(*FOG, VIEW->sceneView, input.globalID.xyz, FOG->fogGridRes, FOG->depthTexture, BindlessSamplers::LinearMinClampEdge());
		
		const float fogNearPlane = FOG->fogNearPlane;
		const float fogFarPlane = FOG->fogFarPlane;
		
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, GetNearPlane(VIEW->sceneView));
		
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, VIEW->sceneView);

		const float density = EvaluateDensityAtLocation(*CONSTS, fogFroxelWorldLocation);

		const float3 fogAlbedo = CONSTS->constantFogAlbedo;

		// Apply constant fog term
		scatteringExtinction += ComputeScatteringAndExtinction(fogAlbedo, CONSTS->constantFogExtinction, density);

		CONSTS->participatingMediaTexture[input.globalID] = scatteringExtinction;
	}
}
