#type(compute) //===========================================================
#include "SculptorShader.hlsli"

[[vk::binding(0, 0)]] RWTexture2D<float4> u_texture;

[numthreads(8, 8, 8)]
void main(int3 id : SV_DispatchThreadID)
{
    const float4 val = u_texture.Load(id.xy);
    u_texture[uint2(id.xy)] = val + float4(1.0, 1.0, 1.0, 1.0);

}