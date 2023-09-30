#include "SculptorShader.hlsli"

[[descriptor_set(VisibilityDataCompressionDS, 0)]]


struct CS_INPUT
{
    uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void VisibilityDataCompressionCS(CS_INPUT input)
{
    const uint2 tile = input.groupID.xy;

    uint2 outputRes;
    u_compressedDataTexture.GetDimensions(outputRes.x, outputRes.y);

    if(tile.x < outputRes.x && tile.y < outputRes.y)
    {
        uint2 inputRes;
        u_inputTexture.GetDimensions(inputRes.x, inputRes.y);

        const uint tilePixelIdx = WaveGetLaneIndex();
        const uint2 tilePixel = uint2(tilePixelIdx & 7, tilePixelIdx >> 3);

        const uint2 inputPixel = tile * uint2(8, 4) + tilePixel;
        const float2 inputUV = (float2(inputPixel) + 0.5f) / float2(inputRes);

        const float inputData = u_inputTexture.SampleLevel(u_nearestSampler, inputUV, 0.0f).x;

        const uint tileData = WaveActiveBallot(inputData > 0.0f).x;

        if(tilePixelIdx == 0)
        {
            u_compressedDataTexture[tile] = tileData;
        }
    }
}
