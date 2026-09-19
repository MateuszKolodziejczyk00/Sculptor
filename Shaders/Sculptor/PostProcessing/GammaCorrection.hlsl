#include "SculptorShader.hlsli"

[[shader_params(GammaCorrectionConstants, PARAMS_GAMMA_CORRECTION)]]

#include "Utils/ColorSpaces.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void GammaCorrectionCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 textureRes = PARAMS_GAMMA_CORRECTION->texture.GetResolution();

    if(pixel.x < textureRes.x && pixel.y < textureRes.y)
    {
        const float4 color = PARAMS_GAMMA_CORRECTION->texture[pixel];
        PARAMS_GAMMA_CORRECTION->texture[pixel] = float4(LinearTosRGB(color.xyz), color.w);
    }
}
