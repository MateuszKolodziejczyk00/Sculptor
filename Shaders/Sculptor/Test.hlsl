#include "SculptorShader.hlsli"

[[descriptor_set(TestDS, 0)]]


[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    u_texture[uint2(id.xy)] = id.x > 500.f ? float4(u_viewInfo.color.xyz + float3(0.2f, 0.f, 1.f), 1.f) : float(float(id.y) / 1080.f).xxxx;
}