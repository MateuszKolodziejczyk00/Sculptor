#include "SculptorShader.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(IndirectInScatteringDS, 1)]]
[[descriptor_set(DDGIDS, 2)]]

#include "DDGI/DDGITypes.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 4)]
void ComputeIndirectInScatteringCS(CS_INPUT input)
{
    uint3 volumetricFogResolution;
    u_inScatteringTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

    if (all(input.globalID < volumetricFogResolution))
    {
        const float3 fogResolutionRcp = rcp(float3(volumetricFogResolution));
        const float3 fogFroxelUVW = (float3(input.globalID) + 0.5f) * fogResolutionRcp;

        const float projectionNearPlane = GetNearPlane(u_sceneView);

        const float fogNearPlane = u_inScatteringParams.fogNearPlane;
        const float fogFarPlane = u_inScatteringParams.fogFarPlane;

        const float prevFogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z - fogResolutionRcp.z, fogNearPlane, fogFarPlane);
        
        const float3 jitter = u_inScatteringParams.jitter;

        const float fogFroxelDepth = fogFroxelUVW.z + jitter.z;
        const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelDepth, fogNearPlane, fogFarPlane);

        const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy + jitter.xy, fogFroxelLinearDepth, projectionNearPlane);

        const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);

        const float3 toViewDir = normalize(u_sceneView.viewLocation - fogFroxelWorldLocation);
        
        float3 inScattering = 0.f;
        const float3 indirectSampleDirections[6] =
        {
            FORWARD_VECTOR,
            UP_VECTOR,
            RIGHT_VECTOR,
            -FORWARD_VECTOR,
            -UP_VECTOR,
            -RIGHT_VECTOR
        };

        [unroll]
        for(uint i = 0; i < 6; ++i)
        {
            float3 direction = indirectSampleDirections[i];
            direction = mul(u_inScatteringParams.randomRotation, float4(direction, 0.f)).xyz;

            DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(fogFroxelWorldLocation, direction, toViewDir);
            ddgiSampleParams.sampleLocationBiasMultiplier = 0.f;
            const float3 indirect = DDGISampleIlluminance(u_ddgiParams, u_probesIlluminanceTexture, u_probesDataSampler, u_probesHitDistanceTexture, u_probesDataSampler, ddgiSampleParams);

            float2 phaseFunctionLUTUV;
            phaseFunctionLUTUV.x = dot(toViewDir, direction) * 0.5f + 0.5f;
            phaseFunctionLUTUV.y = u_inScatteringParams.phaseFunctionAnisotrophy;
            
            const float phaseFunction = u_ddgiScatteringPhaseFunctionLUT.SampleLevel(u_phaseFunctionLUTSampler, phaseFunctionLUTUV, 0.f);

            inScattering += indirect * phaseFunction;
        }

        // We need to normalize it because we sample only 6 perpendicullar directions
        // This is based on the fact that ddgi uses max(0.f, dot(normal, dir)) as weight function
        const float normalizationFactor = 2.f / 3.f;

        u_inScatteringTexture[input.globalID] = inScattering;
    }
}
