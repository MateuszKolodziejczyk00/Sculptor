#include "SculptorShader.hlsli"

[[descriptor_set(LensFlaresPassDS, 0)]]

// Based on http://john-chapman-graphics.blogspot.com/2013/02/pseudo-lens-flare.html


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ComputeLensFlaresCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 outputPixelSize = 1.f / float2(outputRes.x, outputRes.y);

        const float2 outputUV = pixel * outputPixelSize + outputPixelSize * 0.5f;
        const float2 inputUV = -outputUV + 1.f;

        const float2 ghostVector = (0.5f - inputUV) * u_lensFlaresParams.ghostsDispersal;

        const float3 threshold = u_lensFlaresParams.linearColorThreshold;

        float3 ghosts = 0.f;

        const float maxDistToScreenCenter = length(float2(0.5f, 0.5f));

        for (int i = 0; i < u_lensFlaresParams.ghostsNum; ++i)
        {
            const float2 sampleUV = frac(inputUV + ghostVector * i);

            float weight = length(sampleUV - 0.5f) / maxDistToScreenCenter;
            weight = pow(1.f - weight, 10.f);
            
            ghosts.r += max(u_inputTexture.SampleLevel(u_linearSampler, sampleUV + ghostVector * u_lensFlaresParams.ghostsDistortion.r, 0).r - threshold.r, 0.f) * weight;
            ghosts.g += max(u_inputTexture.SampleLevel(u_linearSampler, sampleUV + ghostVector * u_lensFlaresParams.ghostsDistortion.g, 0).g - threshold.g, 0.f) * weight;
            ghosts.b += max(u_inputTexture.SampleLevel(u_linearSampler, sampleUV + ghostVector * u_lensFlaresParams.ghostsDistortion.b, 0).b - threshold.b, 0.f) * weight;
        }

        float3 result = ghosts * u_lensFlaresParams.ghostsIntensity;

        if(length(ghostVector) > 0.01f)
        {
            const float2 haloVector = normalize(ghostVector) * u_lensFlaresParams.haloWidth;

            const float2 haloSample = frac(inputUV + haloVector);
            float weight = length(haloSample - 0.5f) / maxDistToScreenCenter;
            weight = pow(1.f - weight, 15.f);
            float3 halo = 0.f;
            halo.r = max(u_inputTexture.SampleLevel(u_linearSampler, haloSample + haloVector * u_lensFlaresParams.haloDistortion.r, 0).r - threshold.r, 0.f);
            halo.g = max(u_inputTexture.SampleLevel(u_linearSampler, haloSample + haloVector * u_lensFlaresParams.haloDistortion.g, 0).g - threshold.g, 0.f);
            halo.b = max(u_inputTexture.SampleLevel(u_linearSampler, haloSample + haloVector * u_lensFlaresParams.haloDistortion.b, 0).b - threshold.b, 0.f);

            result += halo * weight * u_lensFlaresParams.haloIntensity;
        }

        u_outputTexture[pixel] = result;
    }
}
