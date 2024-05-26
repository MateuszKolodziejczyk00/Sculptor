#include "SculptorShader.hlsli"

[[descriptor_set(GammaCorrectionDS, 0)]]

#include "Utils/ColorSpaces.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void GammaCorrectionCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 textureRes;
    u_texture.GetDimensions(textureRes.x, textureRes.y);

    if(pixel.x < textureRes.x && pixel.y < textureRes.y)
    {
        const float4 color = u_texture[pixel];
        u_texture[pixel] = float4(LinearTosRGB(color.xyz), color.w);
    }
}