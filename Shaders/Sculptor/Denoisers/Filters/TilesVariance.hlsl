#include "SculptorShader.hlsli"

[[descriptor_set(TilesVarianceDS, 0)]]


#define TILE_SIZE_X 16
#define TILE_SIZE_Y 16


groupshared float2 groupMoments[TILE_SIZE_X][TILE_SIZE_Y];


float2 ComputeMoments2x2(uint2 localIdx)
{
    const float2 sum = groupMoments[localIdx.x + 0][localIdx.y + 0]
                     + groupMoments[localIdx.x + 0][localIdx.y + 1]
                     + groupMoments[localIdx.x + 1][localIdx.y + 0]
                     + groupMoments[localIdx.x + 1][localIdx.y + 1];

    return sum * 0.25f;
}


struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 groupID   : SV_GroupID;
    uint3 localID   : SV_GroupThreadID;
};


[numthreads(TILE_SIZE_X, TILE_SIZE_Y, 1)]
void TilesVarianceCS(CS_INPUT input)
{
    uint2 inputRes;
    u_inputValueTexture.GetDimensions(inputRes.x, inputRes.y);

    const uint2 pixel = input.globalID.xy;
    const float2 uv = saturate(float2((float(pixel.x) + 0.5f) / float(inputRes.x), (float(pixel.y) + 0.5f) / float(inputRes.y)));

    uint2 localPixel = input.localID.xy;
    
    const float sampleValue = u_inputValueTexture.SampleLevel(u_nearestSampler, uv, 0).x;
    float2 moments = float2(sampleValue, Pow2(sampleValue));
    groupMoments[localPixel.x][localPixel.y] = moments;

    // Iteration 1

    GroupMemoryBarrierWithGroupSync();

    bool isRelevantInvocation = ((pixel.x | pixel.y) & 1) == 0;
    if(isRelevantInvocation)
    {
        moments = ComputeMoments2x2(localPixel);
    }

    // Iteration 2
    localPixel >>= 1;
    
    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupMoments[localPixel.x][localPixel.y] = moments;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 3) == 0;
    if(isRelevantInvocation)
    {
        moments = ComputeMoments2x2(localPixel);
    }

    // Iteration 3
    localPixel >>= 1;

    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupMoments[localPixel.x][localPixel.y] = moments;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 7) == 0;
    if(isRelevantInvocation)
    {
        moments = ComputeMoments2x2(localPixel);
        //const float variance = abs(moments.y - Pow2(moments.x));
        //u_tilesVarianceTexture[input.groupID.xy] = variance;
    }

    // Iteration 4
    localPixel >>= 1;

    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupMoments[localPixel.x][localPixel.y] = moments;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 15) == 0;
    if(isRelevantInvocation)
    {
        moments = ComputeMoments2x2(localPixel);
        const float variance = abs(moments.y - Pow2(moments.x));
        u_tilesVarianceTexture[input.groupID.xy] = variance;
    }
}