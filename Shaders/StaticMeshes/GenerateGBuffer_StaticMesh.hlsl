#include "SculptorShader.hlsli"
#include "GBuffer/GBuffer.hlsli"

[[descriptor_set(StaticMeshRenderingDS, 0)]]
[[descriptor_set(GeometryDS, 1)]]

struct VS_INPUT
{
    uint index : SV_VertexID;
    [[vk::builtin("DrawIndex")]] uint drawIndex : DRAW_IDX;
};

struct VS_OUTPUT
{
    float4 clipSpace : SV_POSITION;
    float3 vertexNormal : WORLD_LOCATION;
};

VS_OUTPUT StaticMeshVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint primitiveIdx = drawCommands[input.drawIndex].primitiveIdx;

    const PrimitiveGeometryInfo primitiveGeometry = primitivesData[primitiveIdx];

    const float4x4 instanceTransform = instanceTransforms[drawCommands[input.drawIndex].transformIdx];

    const uint index = geometryData.Load<uint>(primitiveGeometry.indicesOffset + input.index * 4);

    const float3 vertexLocation = geometryData.Load<float3>(primitiveGeometry.locationsOffset + index * 12);
    const float3 vertexWorldLocation = mul(float4(vertexLocation, 1.f), instanceTransform);

    float3 vertexNormal = geometryData.Load<float3>(primitiveGeometry.normalsOffset + index * 12);
    vertexNormal = mul(float4(vertexNormal, 0.f), instanceTransform);

    output.clipSpace = mul(float4(vertexWorldLocation, 1.f), sceneViewData.viewProjectionMatrix);
    output.vertexNormal = vertexNormal;

    return output;
}

GBUFFER_OUTPUT StaticMeshFS(VS_OUTPUT vertexInput)
{
    GBUFFER_OUTPUT output;

    const float3 color = normalize(vertexInput.vertexNormal) * 0.5f + 0.5f;
    output.testColor = float4(color, 1.f);
    return output;
}
