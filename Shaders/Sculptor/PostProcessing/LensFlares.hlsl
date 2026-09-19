#include "SculptorShader.hlsli"

[[shader_params(LensFlaresParams, PARAMS_LENS_FLARES_PASS)]]

// Based on http://john-chapman-graphics.blogspot.com/2013/02/pseudo-lens-flare.html


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ComputeLensFlaresCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes = PARAMS_LENS_FLARES_PASS->outputTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 outputPixelSize = 1.f / float2(outputRes.x, outputRes.y);

        const float2 outputUV = pixel * outputPixelSize + outputPixelSize * 0.5f;
        const float2 inputUV = -outputUV + 1.f;

        const float2 ghostVector = (0.5f - inputUV) * PARAMS_LENS_FLARES_PASS->ghostsDispersal;

        float4 ghosts = 0.f;

        const float maxDistToScreenCenter = length(float2(0.5f, 0.5f));

        for (int i = 0; i < PARAMS_LENS_FLARES_PASS->ghostsNum; ++i)
        {
            const float2 sampleUV = frac(inputUV + ghostVector * i);

            float weight = length(sampleUV - 0.5f) / maxDistToScreenCenter;
            weight = pow(1.f - weight, 10.f);
            
            ghosts.r += PARAMS_LENS_FLARES_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV + ghostVector * PARAMS_LENS_FLARES_PASS->ghostsDistortion.r, 0).r * weight;
            ghosts.g += PARAMS_LENS_FLARES_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV + ghostVector * PARAMS_LENS_FLARES_PASS->ghostsDistortion.g, 0).g * weight;
            ghosts.b += PARAMS_LENS_FLARES_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV + ghostVector * PARAMS_LENS_FLARES_PASS->ghostsDistortion.b, 0).b * weight;
            ghosts.w += weight;
        }

        float4 result = ghosts * PARAMS_LENS_FLARES_PASS->ghostsIntensity;

        if(length(ghostVector) > 0.01f)
        {
            const float2 haloVector = normalize(ghostVector) * PARAMS_LENS_FLARES_PASS->haloWidth;

            const float2 haloSample = frac(inputUV + haloVector);

            float weight = length(haloSample - 0.5f) / maxDistToScreenCenter;
            weight = pow(1.f - weight, 10.f);

            float3 halo = 0.f;
            halo.r = PARAMS_LENS_FLARES_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), haloSample + haloVector * PARAMS_LENS_FLARES_PASS->haloDistortion.r, 0).r;
            halo.g = PARAMS_LENS_FLARES_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), haloSample + haloVector * PARAMS_LENS_FLARES_PASS->haloDistortion.g, 0).g;
            halo.b = PARAMS_LENS_FLARES_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), haloSample + haloVector * PARAMS_LENS_FLARES_PASS->haloDistortion.b, 0).b;

            result += float4(halo * weight, weight) * PARAMS_LENS_FLARES_PASS->haloIntensity;
        }

        result.rgb /= max(result.w, 1.f);
        result.rgb = pow(result.rgb, 1.f / 1.8f);
        result.rgb *= PARAMS_LENS_FLARES_PASS->lensFlaresColor;

        PARAMS_LENS_FLARES_PASS->outputTexture[pixel] = result.rgb;
    }
}
