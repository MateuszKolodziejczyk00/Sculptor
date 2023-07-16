#include "SculptorShader.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"

#ifndef ENABLE_RAY_TRACING
#error "ENABLE_RAY_TRACING must be defined"
#endif // ENABLE_RAY_TRACING


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(RenderSceneDS, 1)]]
[[descriptor_set(ViewShadingInputDS, 2)]]
[[descriptor_set(ShadowMapsDS, 3)]]
[[descriptor_set(ComputeInScatteringDS, 4)]]

#if ENABLE_RAY_TRACING
[[descriptor_set(InScatteringAccelerationStructureDS, 5)]]
#endif // ENABLE_RAY_TRACING

#include "Lights/Lighting.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float3 ComputeDirectionalLightsInScattering(in InScatteringParams params)
{
    float3 inScattering = 0.f;
    
    for (uint i = 0; i < u_lightsData.directionalLightsNum; ++i)
    {
        const DirectionalLightGPUData directionalLight = u_directionalLights[i];

        const float3 illuminance = directionalLight.color * directionalLight.illuminance;

        if (any(illuminance > 0.f))
        {
            bool isInShadow = false;

#if ENABLE_RAY_TRACING

            if(u_volumetricRayTracedShadowsParams.enableDirectionalLightsVolumetricRTShadows)
            {
                RayDesc rayDesc;
                rayDesc.Origin      = params.worldLocation;
                rayDesc.Direction   = -directionalLight.direction;
                rayDesc.TMin        = u_volumetricRayTracedShadowsParams.shadowRayMinT;
                rayDesc.TMax        = u_volumetricRayTracedShadowsParams.shadowRayMaxT;
                
                RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadowRayQuery;
                shadowRayQuery.TraceRayInline(u_sceneAccelerationStructure, 0, 0xFF, rayDesc);

                while (shadowRayQuery.Proceed()) {};
                
                isInShadow = (shadowRayQuery.CommittedStatus() != COMMITTED_NOTHING);
            }

#endif // ENABLE_RAY_TRACING

            if (!isInShadow)
            {
                inScattering += illuminance * PhaseFunction(params.toViewNormal, directionalLight.direction, params.phaseFunctionAnisotrophy);
            }
        }
    }

    inScattering *= params.inScatteringColor;

    return inScattering;
}


[numthreads(4, 4, 4)]
void ComputeInScatteringCS(CS_INPUT input)
{
    uint3 volumetricFogResolution;
    u_inScatteringTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

    if (all(input.globalID < volumetricFogResolution))
    {
        const float3 fogResolutionRcp = rcp(float3(volumetricFogResolution));
        const float3 fogFroxelUVW = (float3(input.globalID) + 0.5f) * fogResolutionRcp;

        const float projectionNearPlane = GetNearPlane(u_sceneView);

        const float geometryDepth = u_depthTexture.SampleLevel(u_depthSampler, fogFroxelUVW.xy, 0);
        const float geometryLinearDepth = ComputeLinearDepth(geometryDepth, u_sceneView);
        
        const float fogNearPlane = u_inScatteringParams.fogNearPlane;
        const float fogFarPlane = u_inScatteringParams.fogFarPlane;

        const float prevFogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelUVW.z - fogResolutionRcp.z, fogNearPlane, fogFarPlane);
        
        if(prevFogFroxelLinearDepth > geometryLinearDepth)
        {
            u_inScatteringTexture[input.globalID] = float4(0.f, 0.f, 0.f, -0.1f);
            return;
        }
        
        const float4 scatteringExtinction = u_participatingMediaTexture.SampleLevel(u_participatingMediaSampler, fogFroxelUVW, 0);

        const float3 jitter = u_inScatteringParams.jitter;

        const float fogFroxelDepth = fogFroxelUVW.z + jitter.z;
        const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelDepth, fogNearPlane, fogFarPlane);

        const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy + jitter.xy, fogFroxelLinearDepth, projectionNearPlane);

        const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);
        
        InScatteringParams params;
        params.uv                       = fogFroxelUVW.xy;
        params.linearDepth              = fogFroxelLinearDepth;
        params.worldLocation            = fogFroxelWorldLocation;
        params.toViewNormal             = normalize(u_sceneView.viewLocation - fogFroxelWorldLocation);
        params.phaseFunctionAnisotrophy = u_inScatteringParams.localLightsPhaseFunctionAnisotrophy;
        params.inScatteringColor        = scatteringExtinction.rgb;

        float3 inScattering = ComputeLocalLightsInScattering(params);

        if(u_inScatteringParams.enableDirectionalLightsInScattering)
        {
            params.phaseFunctionAnisotrophy = u_inScatteringParams.dirLightsPhaseFunctionAnisotrophy;
            inScattering += ComputeDirectionalLightsInScattering(params);
        }

        float4 inScatteringExtinction = float4(inScattering, scatteringExtinction.w);

        if (u_inScatteringParams.hasValidHistory)
        {
            const float fogFroxelDepthNoJitter = fogFroxelUVW.z;
            const float fogFroxelLinearDepthNoJitter = ComputeFogFroxelLinearDepth(fogFroxelDepthNoJitter, fogNearPlane, fogFarPlane);

            const float3 fogFroxelNDCNoJitter = FogFroxelToNDC(fogFroxelUVW.xy, fogFroxelLinearDepthNoJitter, projectionNearPlane);
            const float3 fogFroxelWorldLocationNoJitter = NDCToWorldSpaceNoJitter(fogFroxelNDCNoJitter, u_sceneView);

            float4 prevFrameClipSpace = mul(u_prevFrameSceneView.viewProjectionMatrixNoJitter, float4(fogFroxelWorldLocationNoJitter, 1.0f));
            
            if (all(prevFrameClipSpace.xy >= -prevFrameClipSpace.w) && all(prevFrameClipSpace.xyz <= prevFrameClipSpace.w) && prevFrameClipSpace.z >= 0.f)
            {
                prevFrameClipSpace.xyz /= prevFrameClipSpace.w;
                const float prevFrameLinearDepth = ComputeLinearDepth(prevFrameClipSpace.z, u_prevFrameSceneView);

                const float3 prevFrameFogFroxelUVW = ComputeFogFroxelUVW(prevFrameClipSpace.xy * 0.5f + 0.5f, prevFrameLinearDepth, fogNearPlane, fogFarPlane);

                const float4 inScatteringExtinctionHistory = u_inScatteringHistoryTexture.SampleLevel(u_inScatteringHistorySampler, prevFrameFogFroxelUVW, 0);

                if (inScatteringExtinctionHistory.w > 0.f)
                {
                    inScatteringExtinction = lerp(inScatteringExtinctionHistory, inScatteringExtinction, u_inScatteringParams.accumulationCurrentFrameWeight);
                }
            }
        }

        u_inScatteringTexture[input.globalID] = inScatteringExtinction;
    }
}
