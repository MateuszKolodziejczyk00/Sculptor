#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"


[[descriptor_set(ApplyVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyVolumetricFogCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_radianceTexture.GetDimensions(outputRes.x, outputRes.y);

    if (all(pixel < outputRes))
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float depth = u_depthTexture.SampleLevel(u_depthSampler, uv, 0);

        float3 fogFroxelUVW = 0.f;
        if(depth > 0.000001f)
        {
            const float linearDepth = ComputeLinearDepth(depth, GetNearPlane(u_sceneView.projectionMatrix));
            fogFroxelUVW = ComputeFogFroxelUVW(uv, linearDepth, u_applyVolumetricFogParams.fogNearPlane, u_applyVolumetricFogParams.fogFarPlane);
        }
        else
        {
            fogFroxelUVW = float3(uv, 1.f);
        }

        // We use froxel that is closer to avoid light leaking
        const float zBias = 0.012f;
        const float4 inScatteringTransmittance = u_integratedInScatteringTexture.SampleLevel(u_integratedInScatteringSampler, fogFroxelUVW - float3(0.f, 0.f, zBias), 0);
        const float3 inScattering = inScatteringTransmittance.rgb;
        const float transmittance = inScatteringTransmittance.w;

        float3 radiance = u_radianceTexture[pixel];
        radiance = radiance * transmittance + inScattering;

        u_radianceTexture[pixel] = radiance;
    }
}
