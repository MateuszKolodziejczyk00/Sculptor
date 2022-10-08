#type(compute) //===========================================================
#include "SculptorShader.hlsli"

[[descriptor_set(TestDS, 0)]]


[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const float4 val = u_texture.Load(id.xy);
    u_texture[uint2(id.xy)] = val + float4(1.0, 1.0, 1.0, 1.0);
}
