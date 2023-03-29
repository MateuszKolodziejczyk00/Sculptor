#include "SculptorShader.hlsli"

[[descriptor_set(MipsBuildPassDS, 0)]]


groupshared float4 groupValues[32][32];


float4 GetAverageValue(uint2 localPixel)
{
    float4 value = 0.f;
    value += groupValues[localPixel.x + 0][localPixel.y + 0] * 0.25f;
    value += groupValues[localPixel.x + 0][localPixel.y + 1] * 0.25f;
    value += groupValues[localPixel.x + 1][localPixel.y + 0] * 0.25f;
    value += groupValues[localPixel.x + 1][localPixel.y + 1] * 0.25f;

    return value;
}


struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 localID   : SV_GroupThreadID;
};


[numthreads(16, 16, 1)]
void BuildMipsCS(CS_INPUT input)
{
    uint2 outputRes;
    u_textureMip0.GetDimensions(outputRes.x, outputRes.y);

    const uint2 pixel = input.globalID.xy;
    const float2 uv = float2((float(pixel.x) + 0.5f) / float(outputRes.x), (float(pixel.y) + 0.5f) / float(outputRes.y));
    
    float4 value = u_inputTexture.SampleLevel(u_inputSampler, uv, 0);
    u_textureMip0[pixel] = value;

    if(u_mipsBuildParams.downsampleMipsNum == 1)
    {
        return;
    }

    // Mip 1
    uint2 localPixel = input.localID.xy;
    
    GroupMemoryBarrierWithGroupSync();
    groupValues[localPixel.x][localPixel.y] = value;
    GroupMemoryBarrierWithGroupSync();

    bool isRelevantInvocation = ((pixel.x | pixel.y) & 1) == 0;
    if(isRelevantInvocation)
    {
        value = GetAverageValue(localPixel);
        u_textureMip1[pixel >> 1] = value;
    }

    if(u_mipsBuildParams.downsampleMipsNum == 2)
    {
        return;
    }

    // Mip 2
    localPixel >>= 1;
    
    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupValues[localPixel.x][localPixel.y] = value;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 3) == 0;
    if(isRelevantInvocation)
    {
        value = GetAverageValue(localPixel);
        u_textureMip2[pixel >> 2] = value;
    }
    
    if(u_mipsBuildParams.downsampleMipsNum == 3)
    {
        return;
    }

    // Mip 3
    localPixel >>= 1;

    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupValues[localPixel.x][localPixel.y] = value;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 7) == 0;
    if(isRelevantInvocation)
    {
        value = GetAverageValue(localPixel);
        u_textureMip3[pixel >> 3] = value;
    }
    
    if(u_mipsBuildParams.downsampleMipsNum == 4)
    {
        return;
    }

    // Mip 4
    localPixel >>= 1;
    
    GroupMemoryBarrierWithGroupSync();
    if(isRelevantInvocation)
    {
        groupValues[localPixel.x][localPixel.y] = value;
    }
    GroupMemoryBarrierWithGroupSync();

    isRelevantInvocation = ((pixel.x | pixel.y) & 15) == 0;
    if(isRelevantInvocation)
    {
        value = GetAverageValue(localPixel);
        u_textureMip4[pixel >> 4] = value;
    }
}
