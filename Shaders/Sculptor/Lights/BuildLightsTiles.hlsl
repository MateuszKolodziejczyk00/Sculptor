#include "SculptorShader.hlsli"
#include "Lights/PointLightSphereVertices.hlsli"
#include "Lights/LightsTiles.hlsli"


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
    
    uint    lightIdx            : LIGHT_IDX;
};


VS_OUTPUT BuildLightsTilesVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint lightIdx = u_lightDraws[input.drawIndex].localLightIdx;
    const uint vertexIdx = input.index;

    const PointLightGPUData pointLight = u_localLights[lightIdx];

    const float3 lightProxyVertLocation = pointLight.location + pointLightSphereLocations[vertexIdx] * pointLight.radius;

    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(lightProxyVertLocation, 1.f));
    output.fragmentClipSpace = output.clipSpace;
    output.lightIdx = lightIdx;

    return output;
}


void BuildLightsTilesPS(VS_OUTPUT vertexInput)
{
    const float2 uv = vertexInput.fragmentClipSpace.xy / vertexInput.fragmentClipSpace.w * 0.5f + 0.5f;

    const uint2 tileCoords = GetLightsTile(uv, u_lightsPassInfo.tileSize);

    const uint tileLights32Offset = GetLightsTileDataOffsetForLight(tileCoords, u_lightsPassInfo.tilesNum, u_lightsPassInfo.localLights32Num, vertexInput.lightIdx);
    const uint lightMask = 1 << (vertexInput.lightIdx % 32);
    InterlockedOr(u_tilesLightsMask[tileLights32Offset], lightMask);
}
