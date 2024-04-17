#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(IndirectInScatteringDS, 1)]]
[[descriptor_set(DDGISceneDS, 2)]]

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

        [unroll]
        for(uint i = 0; i < 6; ++i)
        {
            const float3 direction = u_inScatteringParams.sampleDirections[i].xyz;

            DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(fogFroxelWorldLocation, direction, toViewDir);
            ddgiSampleParams.sampleLocationBiasMultiplier = 0.f;
            ddgiSampleParams.minVisibility                = 0.9f;
            const float3 indirect = DDGISampleIlluminance(ddgiSampleParams);

            float2 phaseFunctionLUTUV;
            phaseFunctionLUTUV.x = dot(toViewDir, direction) * 0.5f + 0.5f;
            phaseFunctionLUTUV.y = u_inScatteringParams.phaseFunctionAnisotrophy;
            
            const float phaseFunction = u_ddgiScatteringPhaseFunctionLUT.SampleLevel(u_phaseFunctionLUTSampler, phaseFunctionLUTUV, 0.f);

            inScattering += indirect * phaseFunction;
        }

        // We need to normalize it because we sample only 6 perpendicular directions
        // This is based on the fact that ddgi uses max(0.f, dot(normal, dir)) as weight function
        const float normalizationFactor = 2.f / 3.f;

        u_inScatteringTexture[input.globalID] = inScattering;
    }
}
