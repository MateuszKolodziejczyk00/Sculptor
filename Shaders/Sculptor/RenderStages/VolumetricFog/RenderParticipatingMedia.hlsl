#include "SculptorShader.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

[[descriptor_set(RenderParticipatingMediaDS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 4)]
void ParticipatingMediaCS(CS_INPUT input)
{
    uint3 volumetricFogResolution;
    u_participatingMediaTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

    if (all(input.globalID < volumetricFogResolution))
    {
        const float scatteringFactor = u_participatingMediaParams.scatteringFactor;
        
        float4 scatteringExtinction = 0.f;

        // Apply constant fog term
        scatteringExtinction += ComputeScatteringAndExtinction(u_participatingMediaParams.constantFogColor, u_participatingMediaParams.constantFogDensity, scatteringFactor);

        u_participatingMediaTexture[input.globalID] = scatteringExtinction;
    }
}
