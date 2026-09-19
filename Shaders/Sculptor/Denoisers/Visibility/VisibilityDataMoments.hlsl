#include "SculptorShader.hlsli"

[[shader_params(VisibilityMomentsComputationParams, PARAMS_VISIBILITY_MOMENTS_COMPUTATION)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void VisibilityDataMomentsCS(CS_INPUT input)
{
    const int2 pixel = input.globalID.xy;
    
    uint2 outputRes = PARAMS_VISIBILITY_MOMENTS_COMPUTATION->momentsTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const int2 tile = int2(pixel.x >> 3, pixel.y >> 2);

        int2 tilesRes = PARAMS_VISIBILITY_MOMENTS_COMPUTATION->compressedDataTexture.GetResolution();

        const uint2 sampleLocalID = uint2(pixel.x & 7, pixel.y & 3);

        uint samplesSum = 0;
        uint samplesCount = 0;

        uint leftMask = ~0;
        uint rightMask = ~0;
        uint topMask = ~0;
        uint bottomMask = ~0;
        {
            {
                const int topTileY = tile.y - 2;
                [unroll]
                for (int y = 0; y < 4; ++y)
                {
                    if (topTileY * 4 + y < pixel.y - 8)
                    {
                        topMask &= ~(255u << (y * 8));
                    }
                }
            }

            {
                const int bottomTileY = tile.y + 2;
                [unroll]
                for (int y = 0; y < 4; ++y)
                {
                    if (bottomTileY * 4 + y > pixel.y + 8)
                    {
                        bottomMask &= ~(255u << (y * 8));
                    }
                }
            }

            const uint columnMask = 1u | (1u << 8) | (1u << 16) | (1u << 24);

            {
                const int leftTileX = tile.x - 1;
                [unroll]
                for(int x = 0; x < 8; ++x)
                {
                    if(leftTileX * 8 + x < pixel.x - 8)
                    {
                        leftMask &= ~(columnMask << x);
                    }
                }
            }

            {
                const int rightTileX = tile.x + 1;
                [unroll]
                for (int x = 0; x < 8; ++x)
                {
                    if (rightTileX * 8 + x > pixel.x + 8)
                    {
                        rightMask &= ~(columnMask << x);
                    }
                }
            }
        }

        [unroll]
        for (int y = -2; y <= 2; ++y)
        {
            const uint yMask = y == -2 ? topMask : y == 2 ? bottomMask : ~0;
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                const uint xMask = x == -1 ? leftMask : x == 1 ? rightMask : ~0;
                const uint mask = yMask & xMask;

                const uint validSamplesNum = countbits(mask);

                const int3 tileCoords = int3(clamp(tile.x + x, 0, tilesRes.x), clamp(tile.y + y, 0, tilesRes.y), 0);
                const uint tileSamples = PARAMS_VISIBILITY_MOMENTS_COMPUTATION->compressedDataTexture.Load(tileCoords).x;

                samplesSum += countbits(tileSamples & mask);
                samplesCount += validSamplesNum;
            }
        }

        const float moments = float(samplesSum) / float(samplesCount);

        PARAMS_VISIBILITY_MOMENTS_COMPUTATION->momentsTexture[pixel] = moments;
    }
}
