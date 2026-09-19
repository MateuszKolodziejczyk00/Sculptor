#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RenderAerialPerspectiveConstants, PARAMS_RENDER_AERIAL_PERSPECTIVE)]]

#include "Atmosphere/AerialPerspective.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID  : SV_GroupID;
    uint3 localID  : SV_GroupThreadID;
};


[numthreads(1, 1, 32)]
void RenderAerialPerspectiveCS(CS_INPUT input)
{
    const AtmosphereParams atmosphere = *PARAMS_RENDER_AERIAL_PERSPECTIVE->atmosphereParams;
    const AerialPerspectiveParams ap = atmosphere.aerialPerspectiveParams;

    const DirectionalLightGPUData dirLight = PARAMS_RENDER_AERIAL_PERSPECTIVE->directionalLights[0];

    const float3 toLightDir = -dirLight.direction;

    const float2 uv = float2(input.globalID.xy + 0.5f) * ap.rcpResolution.xy;

    const float3 viewDir = ComputeViewRayDirectionWS(VIEW->sceneView, uv);
    const float cosTheta = dot(viewDir, toLightDir);

    const float miePhaseValue              = GetMiePhase(cosTheta);
    const float rayleighPhaseValue         = GetRayleighPhase(-cosTheta);
    const float rayleighPhaseValueIndirect = 1.f / (4.f * PI);

    float3 accumulatedTranmittance = 1.f;
    float3 accumulatedInScattering = 0.f;

    for(uint sliceIdx = WaveGetLaneIndex(); sliceIdx < ap.resolution.z; sliceIdx += WaveGetLaneCount())
    {
        const uint3 coords = uint3(input.globalID.xy, sliceIdx);
        const float3 uvw = float3(uv, (coords.z + 0.5f) * ap.rcpResolution.z);

        const float linearDepth = ComputeAPLinearDepth(ap, uvw.z);

        const float3 worldLocation = APToWS(ap, VIEW->sceneView, uvw.xy, linearDepth);

        const float dt = (linearDepth - ComputeAPLinearDepth(ap, max(uvw.z - ap.rcpResolution.z, 0.0f))) * 0.000001f; // must be in mega meters

        const float3 atmosphereLocation = GetLocationInAtmosphere(atmosphere, worldLocation);

        const ScatteringValues scatteringValues = ComputeScatteringValues(atmosphere, atmosphereLocation);

        const float3 sampleTransmittance = exp(-dt * scatteringValues.extinction);

        const float3 pmUVW = ComputeFogFroxelUVW(uv, linearDepth, PARAMS_RENDER_AERIAL_PERSPECTIVE->participatingMediaNear, PARAMS_RENDER_AERIAL_PERSPECTIVE->participatingMediaFar);

        const float shadowTerm = PARAMS_RENDER_AERIAL_PERSPECTIVE->dirLightShadowTerm.SampleLevel(BindlessSamplers::LinearClampEdge(), pmUVW, 0.f);
        const float3 lightTransmittance = GetTransmittanceFromLUT(atmosphere, PARAMS_RENDER_AERIAL_PERSPECTIVE->transmittanceLUT, BindlessSamplers::LinearClampEdge(), atmosphereLocation, toLightDir);

        const float3 directIlluminance = dirLight.outerSpaceIlluminance * lightTransmittance * shadowTerm;
        const float3 indirectIlluminance = PARAMS_RENDER_AERIAL_PERSPECTIVE->indirectInScatteringTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), pmUVW, 0.f);

        const float3 rayleighScattering         = scatteringValues.rayleighScattering * rayleighPhaseValue;
        const float3 rayleighScatteringIndirect = scatteringValues.rayleighScattering * rayleighPhaseValueIndirect;
        const float3 mieScattering              = scatteringValues.mieScattering * miePhaseValue;

        const float3 inScattering = rayleighScattering + mieScattering;

        const float3 scatteringIntegral = (inScattering / scatteringValues.extinction) * (1.f - sampleTransmittance);
        const float3 indirectScatteringIntegral = (rayleighScatteringIndirect / scatteringValues.rayleighExtinction) * (1.f - sampleTransmittance);

        const float3 previousTransmittance = accumulatedTranmittance * WavePrefixProduct(sampleTransmittance);
        const float3 transmittance = previousTransmittance * sampleTransmittance;

        const float3 deltaInScatteredLight = (scatteringIntegral * directIlluminance + indirectScatteringIntegral * indirectIlluminance) * previousTransmittance;
        const float3 integratedInScattering = accumulatedInScattering + WavePrefixSum(deltaInScatteredLight) + deltaInScatteredLight;

        PARAMS_RENDER_AERIAL_PERSPECTIVE->rwAerialPerspective[coords] = float4(LuminanceToExposedLuminance(integratedInScattering), dot(transmittance, 0.333f));

        accumulatedTranmittance = WaveReadLaneAt(transmittance, WaveGetLaneCount() - 1u);
        accumulatedInScattering = WaveReadLaneAt(integratedInScattering, WaveGetLaneCount() - 1u);
    }
}