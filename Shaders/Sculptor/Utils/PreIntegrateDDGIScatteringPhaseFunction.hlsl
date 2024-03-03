#include "SculptorShader.hlsli"

[[descriptor_set(PreIntegrateDDGIScatteringPhaseFunctionDS, 0)]]

#include "Lights/LightingUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float PreIntegrateDDGIScatteringPhaseFunction(float NdotV, float phaseFunctionAnisotrophy)
{
    float3 V = float3(0.f, 0.f, NdotV);
    V.x = sqrt(1.f - V.z * V.z);

    float phaseFunctionSum = 0.f;

    const uint sampleCount = 10000;
    for(uint i = 0u; i < sampleCount; ++i)
    {
        const float2 random = Hammersley(i, sampleCount);

        const float theta = acos(sqrt(1.f - random.x));
        const float phi = 2.f * PI * random.y;

        float cosTheta = 0.f;
        float sinTheta = 0.f;
        float cosPhi = 0.f;
        float sinPhi = 0.f;

        sincos(phi, sinPhi, cosPhi);
        sincos(theta, sinTheta, cosTheta);

        const float3 vec = float3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);

        phaseFunctionSum += PhaseFunction(V, vec, phaseFunctionAnisotrophy);
    }

    phaseFunctionSum += PhaseFunction(V, V, phaseFunctionAnisotrophy);

    return phaseFunctionSum / float(sampleCount + 1u);
}


[numthreads(8, 8, 1)]
void PreIntegrateDDGIScatteringPhaseFunctionCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_lut.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (pixel + 0.5f) / outputRes;
        u_lut[pixel] = PreIntegrateDDGIScatteringPhaseFunction(2.f * uv.x - 1.f, uv.y);
    }
}
