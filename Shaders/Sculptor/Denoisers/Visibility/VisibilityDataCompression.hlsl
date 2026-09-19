#include "SculptorShader.hlsli"

[[shader_params(VisibilityDataCompressionParams, PARAMS_VISIBILITY_DATA_COMPRESSION)]]


struct CS_INPUT
{
    uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void VisibilityDataCompressionCS(CS_INPUT input)
{
    const uint2 tile = input.groupID.xy;

    uint2 outputRes = PARAMS_VISIBILITY_DATA_COMPRESSION->compressedDataTexture.GetResolution();

    if(tile.x < outputRes.x && tile.y < outputRes.y)
    {
        uint2 inputRes = PARAMS_VISIBILITY_DATA_COMPRESSION->inputTexture.GetResolution();

        const uint tilePixelIdx = WaveGetLaneIndex();
        const uint2 tilePixel = uint2(tilePixelIdx & 7, tilePixelIdx >> 3);

        const uint2 inputPixel = tile * uint2(8, 4) + tilePixel;
        const float2 inputUV = (float2(inputPixel) + 0.5f) / float2(inputRes);

        const float inputData = PARAMS_VISIBILITY_DATA_COMPRESSION->inputTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), inputUV, 0.0f).x;

        const uint tileData = WaveActiveBallot(inputData > 0.0f).x;

        if(tilePixelIdx == 0)
        {
            PARAMS_VISIBILITY_DATA_COMPRESSION->compressedDataTexture[tile] = tileData;
        }
    }
}
