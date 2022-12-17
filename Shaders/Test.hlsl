#type(compute) //===========================================================
#include "SculptorShader.hlsli"

[[descriptor_set(TestDS, 0)]]


[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    u_texture[uint2(id.xy)] = float4(u_viewInfo.color.xyz + float3(0.2f, 1.f, 0.f), 1.f);
}