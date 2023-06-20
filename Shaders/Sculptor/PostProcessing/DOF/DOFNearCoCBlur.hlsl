#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DOFNearCoCBlurDS, 0)]]


#define DOF_BLUR_OP_MAX 1
#define DOF_BLUR_OP_AVG 2


#ifndef DOF_BLUR_OP

#error "DOF_BLUR_OP must be defined"

#endif // DOF_BLUR_OP


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFNearCoCBlurCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_cocTextureBlurred.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (float2(pixel) + 0.5f) / float2(outputRes);
        const float2 pixelSize = rcp(float2(outputRes));

        const int blurSize = 6;

        float2 direction = u_params.isHorizontal ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);
        direction *= pixelSize;

        const float2 center = u_cocTexture.SampleLevel(u_nearestSampler, uv, 0.f);

#if DOF_BLUR_OP == DOF_BLUR_OP_AVG
 
        float sum = 0.0f;

        for (int offset = 1; offset <= blurSize; ++offset)
        {
            sum += u_cocTexture.SampleLevel(u_nearestSampler, uv + offset * direction, 0.f).r;
            sum += u_cocTexture.SampleLevel(u_nearestSampler, uv - offset * direction, 0.f).r;
        }

        float newValue = center.x + sum;
        newValue /= (2 * blurSize + 1);

#elif DOF_BLUR_OP == DOF_BLUR_OP_MAX
                
        float newValue = center.x;

        for (int offset = 1; offset <= blurSize; ++offset)
        {
            newValue = max(u_cocTexture.SampleLevel(u_nearestSampler, uv + offset * direction, 0.f).r, newValue);
            newValue = max(u_cocTexture.SampleLevel(u_nearestSampler, uv - offset * direction, 0.f).r, newValue);
        }

#endif // DOF_BLUR_OP

        u_cocTextureBlurred[pixel] = newValue;
    }
}
