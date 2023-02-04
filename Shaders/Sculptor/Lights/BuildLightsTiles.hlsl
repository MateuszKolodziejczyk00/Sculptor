#include "SculptorShader.hlsli"
#include "Lights/PointLightSphereVertices.hlsli"


[[descriptor_set(BuildLightTilesDS, 0)]]


struct VS_INPUT
{
    [[vk::builtin("DrawIndex")]] uint drawIndex : DRAW_INDEX;
    uint index : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace           : SV_POSITION;
    
    float4  fragmentClipSpace   : CLIP_SPACE;
};


VS_OUTPUT BuildLightsTilesVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint lightIdx = u_lightDraws[input.drawIndex].localLightIdx;
    const uint vertexIdx = input.index;

    const PointLightGPUData pointLight = u_localLights[lightIdx];

    const float3 lightProxyVertLocation = pointLight.location + pointLightSphereLocations[vertexIdx];

    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(lightProxyVertLocation, 1.f));
    output.fragmentClipSpace = output.clipSpace;

    return output;
}


void BuildLightsTilesPS(VS_OUTPUT vertexInput)
{
    const float2 uv = vertexInput.fragmentClipSpace.xy / vertexInput.fragmentClipSpace.w * 0.5f + 0.5f;

    const uint2 tileCoords = uint2(uv / float2(u_lightsPassInfo.tileSizeX, u_lightsPassInfo.tileSizeY));
}
