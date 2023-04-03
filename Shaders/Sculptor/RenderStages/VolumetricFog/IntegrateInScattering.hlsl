#include "SculptorShader.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(IntegrateInScatteringDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void IntegrateInScatteringCS(CS_INPUT input)
{
    uint3 volumetricFogResolution;
    u_integratedInScatteringTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

    const float3 rcpFogResolution = rcp(float3(volumetricFogResolution));

    if (all(input.globalID.xy < volumetricFogResolution.xy))
    {
        const float2 uv = (input.globalID.xy + 0.5f) * rcpFogResolution.xy;
        
        const float nearPlane = GetNearPlane(u_sceneView.projectionMatrix);
        const float geometryLinearDepth = ComputeLinearDepth(u_depthTexture.SampleLevel(u_depthSampler, uv, 0).x, nearPlane);
        
        float currentZ = 0.f;

        float3 integratedScattering = 0.f;
        float integratedTransmittance = 1.f;

        for (uint z = 0; z < volumetricFogResolution.z && currentZ <= geometryLinearDepth; ++z)
        {
            const float fogFroxelDepth = (z + 0.5f) * rcpFogResolution.z;
            const float nextZ = ComputeFogFroxelLinearDepth(fogFroxelDepth, u_integrateInScatteringParams.fogNearPlane, u_integrateInScatteringParams.fogFarPlane);
            
            const float4 inScatteringExtinction = u_inScatteringTexture.SampleLevel(u_inScatteringSampler, float3(uv, z * rcpFogResolution.z), 0);

            const float3 inScattering = inScatteringExtinction.rgb;
            const float extinction = inScatteringExtinction.a;

            const float clampedExtinction = max(extinction, 0.0001f);

            const float transmittance = exp(-clampedExtinction * (nextZ - currentZ));

            const float3 scattering = (inScattering * (1.f - transmittance)) / clampedExtinction;

            integratedScattering += scattering * integratedTransmittance;
            integratedTransmittance *= transmittance;

            currentZ = nextZ;
        
            u_integratedInScatteringTexture[uint3(input.globalID.xy, z)] = float4(integratedScattering, integratedTransmittance);
        }
    }
}
