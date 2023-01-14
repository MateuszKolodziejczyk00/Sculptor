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
    float3 worldLocation : WORLD_LOCATION;
    float4 clipSpace : SV_POSITION;
};

VS_OUTPUT StaticMeshVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint primitiveIdx = drawCommands[input.drawIndex].primitiveIdx;

    const PrimitiveGeometryInfo primitiveGeometry = primitivesData[primitiveIdx];

    const uint index = geometryData.Load(primitiveGeometry.indicesOffset + input.index * 4);

    const uint3 test = geometryData.Load3(primitiveGeometry.locationsOffset + index * 12);
    const float3 vertexLocation = float3(asfloat(test.x), asfloat(test.y), asfloat(test.z));
    // TODO
    //const float3 vertexWorldLocation = mul(float3(vertexLocation, 1.f), )

    output.clipSpace = mul(float4(vertexLocation, 1.f), sceneViewData.viewProjectionMatrix);
    output.worldLocation = vertexLocation;

    return output;
}

GBUFFER_OUTPUT StaticMeshFS(VS_OUTPUT vertexInput)
{
    GBUFFER_OUTPUT output;

    output.testColor = float4((vertexInput.worldLocation + float3(2000.f, 2000.f, 2000.f)) / 4000.f, 1.f);
    return output;
}
