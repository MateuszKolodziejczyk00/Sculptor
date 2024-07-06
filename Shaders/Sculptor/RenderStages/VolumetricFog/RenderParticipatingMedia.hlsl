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
	const float noiseThreshold = params.densityNoiseThreshold + exp(params.densityNoiseZSigma / location.z) * (1.f - params.densityNoiseThreshold);
	const float maxDensity = 0.5f;

	const float3 noiseLocation = location + params.densityNoiseSpeed * u_gpuSceneFrameConstants.time;

	const float lowFrequencyOctave = ComputeDensityNoise(noiseLocation * 0.7f) * 0.9f;
	const float highFrequencyOctave = ComputeDensityNoise(noiseLocation * 3.f) * 0.1f;

	float noise = lowFrequencyOctave + highFrequencyOctave;
	noise = saturate(noise - noiseThreshold) / max(1.f - noiseThreshold, 0.001f);

	return saturate(lerp(params.minDensity, params.maxDensity, noise));
}


[numthreads(4, 4, 4)]
void ParticipatingMediaCS(CS_INPUT input)
{
	if (all(input.globalID < u_fogConstants.fogGridRes))
	{
		const float scatteringFactor = u_participatingMediaParams.scatteringFactor;
		
		float4 scatteringExtinction = 0.f;

		const float3 fogFroxelUVW = ComputeFogGridSampleUVW(u_fogConstants, u_sceneView, input.globalID.xyz, u_fogConstants.fogGridRes, u_depthTexture, u_depthSampler);
		
		const float fogNearPlane = u_fogConstants.fogNearPlane;
		const float fogFarPlane = u_fogConstants.fogFarPlane;
		
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, GetNearPlane(u_sceneView));
		
		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);

		const float density = EvaluateDensityAtLocation(u_participatingMediaParams, fogFroxelWorldLocation);

		// Apply constant fog term
		scatteringExtinction += ComputeScatteringAndExtinction(u_participatingMediaParams.constantFogColor, density, scatteringFactor);

		u_participatingMediaTexture[input.globalID] = scatteringExtinction;
	}
}
