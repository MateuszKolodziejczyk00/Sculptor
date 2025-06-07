#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(RenderAerialPerspectiveDS, 1)]]

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
    const AtmosphereParams atmosphere = u_atmosphereParams;
    const AerialPerspectiveParams ap = atmosphere.aerialPerspectiveParams;

    const DirectionalLightGPUData dirLight = u_directionalLights[0];

    const float3 toLightDir = -dirLight.direction;

    const float2 uv = float2(input.globalID.xy + 0.5f) * ap.rcpResolution.xy;

    const float3 viewDir = ComputeViewRayDirectionWS(u_sceneView, uv);
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

        const float3 worldLocation = APToWS(ap, u_sceneView, uvw.xy, linearDepth);

        const float dt = (linearDepth - ComputeAPLinearDepth(ap, max(uvw.z - ap.rcpResolution.z, 0.0f))) * 0.000001f; // must be in mega meters

        const float3 atmosphereLocation = GetLocationInAtmosphere(atmosphere, worldLocation);

        const ScatteringValues scatteringValues = ComputeScatteringValues(atmosphere, atmosphereLocation);
    
        const float3 sampleTransmittance = exp(-dt * scatteringValues.extinction);

        const float3 pmUVW = ComputeFogFroxelUVW(uv, linearDepth, u_renderAPConstants.participatingMediaNear, u_renderAPConstants.participatingMediaFar);

        const float shadowTerm = u_dirLightShadowTerm.SampleLevel(u_linearSampler, pmUVW, 0.f);
        const float3 lightTransmittance = GetTransmittanceFromLUT(atmosphere, u_transmittanceLUT, u_linearSampler, atmosphereLocation, toLightDir);

        const float3 directIlluminance = dirLight.outerSpaceIlluminance * lightTransmittance * shadowTerm;
        const float3 indirectIlluminance = u_indirectInScatteringTexture.SampleLevel(u_linearSampler, pmUVW, 0.f);

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

        u_rwAerialPerspective[coords] = float4(LuminanceToExposedLuminance(integratedInScattering), dot(transmittance, 0.333f));

        accumulatedTranmittance = WaveReadLaneAt(transmittance, WaveGetLaneCount() - 1u);
        accumulatedInScattering = WaveReadLaneAt(integratedInScattering, WaveGetLaneCount() - 1u);
    }
}