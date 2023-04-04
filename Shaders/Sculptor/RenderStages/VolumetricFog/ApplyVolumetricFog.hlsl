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

            // We use froxel that is closer to avoid light leaking
            const float zBias = 0.009f;
            fogFroxelUVW.z -= zBias;
        }
        else
        {
            fogFroxelUVW = float3(uv, 1.f);
        }

        float4 inScatteringTransmittance = u_integratedInScatteringTexture.SampleLevel(u_integratedInScatteringSampler, fogFroxelUVW, 0);

        float weight = 1.f;

        [unroll]
        for (int i = -1; i <= 1; ++i)
        {
            [unroll]
            for (int j = -1; j <= 1; ++j)
            {
                const float2 currentSampleUV = uv + float2(i, j) * pixelSize * 4.f;

                const float sampleDepth = u_depthTexture.SampleLevel(u_depthSampler, currentSampleUV, 0);

                inScatteringTransmittance += u_integratedInScatteringTexture.SampleLevel(u_integratedInScatteringSampler, float3(currentSampleUV, fogFroxelUVW.z), 0);
                weight += 1.f;
            }
        }
         
        inScatteringTransmittance /= weight;

        const float3 inScattering = inScatteringTransmittance.rgb;
        const float transmittance = inScatteringTransmittance.a;

        float3 radiance = u_radianceTexture[pixel];
        radiance = radiance * transmittance + inScattering;

        u_radianceTexture[pixel] = radiance;
    }
}
