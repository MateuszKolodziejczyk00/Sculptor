#include "SculptorShader.hlsli"
#include "GBuffer/GBuffer.hlsli"

[[descriptor_set(StaticMeshRenderingVSDS, 0)]]

struct VS_INPUT
{
    uint vertexIdx : SV_VertexID;
    [[vk::builtin("DrawIndex")]] uint drawIndex : DRAW_IDX;
};

struct VS_OUTPUT
{
    float4 clipSpace : SV_POSITION;
};

VS_OUTPUT StaticMeshVS(VS_INPUT input)
{
    VS_OUTPUT output;
      
    float3 vertexLocation = float3(1.f, 1.f, 3.f);
    output.clipSpace = mul(float4(vertexLocation, 1.f), sceneViewData.viewProjectionMatrix);

    return output;
}

GBUFFER_OUTPUT StaticMeshFS(VS_OUTPUT vertexInput)
{
    GBUFFER_OUTPUT output;

    output.testColor = float4(1.f, 0.f, 1.f, 1.f);
    return output;
}
