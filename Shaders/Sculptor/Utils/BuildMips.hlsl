#include "SculptorShader.hlsli"

[[shader_params(MipsBuildPassParams, PARAMS_MIPS_BUILD_PASS)]]


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
    uint2 outputRes = PARAMS_MIPS_BUILD_PASS->textureMip0.GetResolution();

    const uint2 pixel = input.globalID.xy;
    const float2 uv = float2((float(pixel.x) + 0.5f) / float(outputRes.x), (float(pixel.y) + 0.5f) / float(outputRes.y));
    
    float4 value = PARAMS_MIPS_BUILD_PASS->inputTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0);
    PARAMS_MIPS_BUILD_PASS->textureMip0[pixel] = value;

    if(PARAMS_MIPS_BUILD_PASS->downsampleMipsNum == 1)
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
        PARAMS_MIPS_BUILD_PASS->textureMip1[pixel >> 1] = value;
    }

    if(PARAMS_MIPS_BUILD_PASS->downsampleMipsNum == 2)
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
        PARAMS_MIPS_BUILD_PASS->textureMip2[pixel >> 2] = value;
    }
    
    if(PARAMS_MIPS_BUILD_PASS->downsampleMipsNum == 3)
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
        PARAMS_MIPS_BUILD_PASS->textureMip3[pixel >> 3] = value;
    }
    
    if(PARAMS_MIPS_BUILD_PASS->downsampleMipsNum == 4)
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
        PARAMS_MIPS_BUILD_PASS->textureMip4[pixel >> 4] = value;
    }
}
