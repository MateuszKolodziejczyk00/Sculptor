#include "SculptorShader.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/PerlinNoise.hlsli"

[[descriptor_set(RenderParticipatingMediaDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(RenderSceneDS, 2)]]


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
    uint3 volumetricFogResolution;
    u_participatingMediaTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

    if (all(input.globalID < volumetricFogResolution))
    {
        const float scatteringFactor = u_participatingMediaParams.scatteringFactor;
        
        float4 scatteringExtinction = 0.f;

        const float3 fogResolutionRcp = rcp(float3(volumetricFogResolution));
        const float3 fogFroxelUVW = (float3(input.globalID) + 0.5f) * fogResolutionRcp;
        
        const float fogNearPlane = u_participatingMediaParams.fogNearPlane;
        const float fogFarPlane = u_participatingMediaParams.fogFarPlane;
        
        const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z, fogNearPlane, fogFarPlane);

        const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepth, GetNearPlane(u_sceneView));
        
        const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);

        const float density = EvaluateDensityAtLocation(u_participatingMediaParams, fogFroxelWorldLocation);

        // Apply constant fog term
        scatteringExtinction += ComputeScatteringAndExtinction(u_participatingMediaParams.constantFogColor, density, scatteringFactor);

        u_participatingMediaTexture[input.globalID] = scatteringExtinction;
    }
}
