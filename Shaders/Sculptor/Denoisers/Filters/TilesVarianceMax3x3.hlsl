#include "SculptorShader.hlsli"

[[descriptor_set(TilesVarianceMax3x3DS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8u, 8u, 1)]
void TilesVarianceMaxCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_tilesVarianceMax3x3Texture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        float maxVariance = 0.0f;

        for(int y = -1; y <= 1; ++y)
        {
            for(int x = -1; x <= 1; ++x)
            {
                const float2 offset = float2(x, y) * pixelSize;
                const float variance = u_tilesVarianceTexture.SampleLevel(u_nearestSampler, uv + offset, 0).x;
                maxVariance = max(maxVariance, variance);
            }
        }

        u_tilesVarianceMax3x3Texture[pixel] = maxVariance;
    }
}
