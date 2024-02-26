#include "SculptorShader.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/Shapes.hlsli"

[[descriptor_set(RenderSkyViewLUTDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float3 RaymarchScattering(in Ray ray, in float3 lightDirection, in float stepsNum, in float distance)
{
    const float cosTheta = dot(ray.direction, lightDirection);

    const float miePhaseValue = GetMiePhase(cosTheta);
    const float rayleighPhaseValue = GetRayleighPhase(-cosTheta);

    const float rcpStepsNum = rcp(stepsNum);

    float3 luminance = 0.f;
    float3 transmittance = 1.f;

    float currentT = 0.f;

    for (float i = 0.f; i < stepsNum; i += 1.f)
    {
        const float newT = (i + 0.3f) * rcpStepsNum * distance;
        const float dt = newT - currentT;
        currentT = newT;

        const float3 sampleLocation =  ray.origin + ray.direction * currentT;

        const ScatteringValues scatteringValues = ComputeScatteringValues(u_atmosphereParams, sampleLocation);

        const float3 sampleTransmittance = exp(-dt * scatteringValues.extinction);

        const float3 lightTransmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, sampleLocation, lightDirection);

        const float3 multiScatteringPsi = GetMultiScatteringPsiFromLUT(u_atmosphereParams, u_multiScatteringLUT, u_linearSampler, sampleLocation, lightDirection);
        
        const float3 rayleighScattering = scatteringValues.rayleighScattering * (rayleighPhaseValue * lightTransmittance + multiScatteringPsi);
        const float3 mieScattering = scatteringValues.mieScattering * (miePhaseValue * lightTransmittance + multiScatteringPsi);

        const float3 inScattering = rayleighScattering + mieScattering;

        const float3 scatteringIntegral = (inScattering / scatteringValues.extinction) * (1.f - sampleTransmittance);

        luminance += scatteringIntegral * transmittance;
        transmittance *= sampleTransmittance;
    }

    return luminance;
}


[numthreads(8, 8, 1)]
void RenderSkyViewLUTCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_skyViewLUT.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (pixel + 0.5f) / float2(outputRes);

        const float azimuthAngle = (uv.x - 0.5f) * 2.f * PI;

        float compressedV;
        if(uv.y < 0.5f)
        {
            const float coord = 1.f - uv.y * 2.f;
            compressedV = -Pow2(coord);
        }
        else
        {
            const float coord = uv.y * 2.f - 1.f;
            compressedV = Pow2(coord);
        }

        const float3 viewLocation = GetLocationInAtmosphere(u_atmosphereParams, u_sceneView.viewLocation);
        const float viewHeight = length(viewLocation);

        const float3 upVector = viewLocation / viewHeight;

        const float distToHorizon = sqrt(Pow2(viewHeight) - Pow2(u_atmosphereParams.groundRadiusMM));
        const float horizonAngle = acos(distToHorizon / viewHeight) - 0.5f * PI;

        const float altitudeAngle = compressedV * 0.5f * PI - horizonAngle;

        float sinAltitude, cosAltitude;
        sincos(altitudeAngle, sinAltitude, cosAltitude);

        float sinAzimuth, cosAzimuth;
        sincos(azimuthAngle, sinAzimuth, cosAzimuth);

        const float3 rayDirection = float3(cosAzimuth * cosAltitude, sinAzimuth * cosAltitude, sinAltitude);

        const Sphere groundSphere     = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.groundRadiusMM);
        const Sphere atmosphereSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.atmosphereRadiusMM);

        const Ray ray = { viewLocation, rayDirection };

        const float atmosphereDist = ray.IntersectSphere(atmosphereSphere).GetTime();
        const float distToGround   = ray.IntersectSphere(groundSphere).GetTime();
        
        const float rayDistance = distToGround > 0.f ? distToGround : atmosphereDist;

        const float raymarchingStepsNum = 32.f;

        float3 skyLuminance = 0.f;

        for (int lightIdx = 0; lightIdx < u_atmosphereParams.directionalLightsNum; ++lightIdx)
        {
            const DirectionalLightGPUData directionalLight = u_directionalLights[lightIdx];

            const float3 lightDirection = -directionalLight.direction;

            skyLuminance += RaymarchScattering(ray, lightDirection, raymarchingStepsNum, rayDistance) * directionalLight.illuminance;
        }

        u_skyViewLUT[pixel] = skyLuminance;
    }
}
