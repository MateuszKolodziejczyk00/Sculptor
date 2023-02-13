#include "SculptorShader.hlsli"

[[descriptor_set(BuildHiZDS, 0)]]


groupshared float groupDepths[16][16];


float GetMinDepth(uint2 localPixel)
{
    float4 depths;
    depths.x = groupDepths[localPixel.x + 0][localPixel.y + 0];
    depths.y = groupDepths[localPixel.x + 0][localPixel.y + 1];
    depths.z = groupDepths[localPixel.x + 1][localPixel.y + 0];
    depths.w = groupDepths[localPixel.x + 1][localPixel.y + 1];

    depths.x = min(depths.x, depths.y);
    depths.z = min(depths.z, depths.w);
    depths.x = min(depths.x, depths.z);

    return depths.x;
}


struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 localID   : SV_GroupThreadID;
};


[numthreads(16, 16, 1)]
void BuildHiZCS(CS_INPUT input)
{
    uint2 outputRes;
    u_HiZMip0.GetDimensions(outputRes.x, outputRes.y);

    const uint2 pixel = input.globalID.xy;
    const float2 uv = float2((float(pixel.x) + 0.5f) / float(outputRes.x), (float(pixel.y) + 0.5f) / float(outputRes.y));
    
    float depth = u_depthTexture.SampleLevel(u_depthSampler, uv, 0).x;
    u_HiZMip0[pixel] = depth;

    if(u_buildParams.downsampleMipsNum == 1)
    {
        return;
    }

    // Mip 1
    uint2 localPixel = input.localID.xy;
    
    GroupMemoryBarrierWithGroupSync();
    groupDepths[localPixel.x][localPixel.y] = depth;
    GroupMemoryBarrierWithGroupSync();

    bool isRelevantInvocation = ((pixel.x | pixel.y) & 1) == 0;
    if(isRelevantInvocation)
    {
        depth = GetMinDepth(localPixel);
        u_HiZMip1[pixel >> 1] = depth;
    }

    if(u_buildParams.downsampleMipsNum == 2)
    {
        return;
    }

    // Mip 2
    localPixel >>= 1;
    
    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupDepths[localPixel.x][localPixel.y] = depth;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 3) == 0;
    if(isRelevantInvocation)
    {
        depth = GetMinDepth(localPixel);
        u_HiZMip2[pixel >> 2] = depth;
    }
    
    if(u_buildParams.downsampleMipsNum == 3)
    {
        return;
    }

    // Mip 3
    localPixel >>= 1;

    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupDepths[localPixel.x][localPixel.y] = depth;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 7) == 0;
    if(isRelevantInvocation)
    {
        depth = GetMinDepth(localPixel);
        u_HiZMip3[pixel >> 3] = depth;
    }
    
    if(u_buildParams.downsampleMipsNum == 4)
    {
        return;
    }

    // Mip 4
    localPixel >>= 1;
    
    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupDepths[localPixel.x][localPixel.y] = depth;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 15) == 0;
    if(isRelevantInvocation)
    {
        depth = GetMinDepth(localPixel);
        u_HiZMip4[pixel >> 4] = depth;
    }
}