#include "SculptorShader.hlsli"


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(BuildLightTilesDS, 1)]]

#include "Lights/LightsTiles.hlsli"
#include "Lights/LightingUtils.hlsli"


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

#if LIGHT_TYPE == LIGHT_TYPE_POINT
    const uint lightIdx = u_pointLightDraws[input.drawIndex].localLightIdx;
#elif LIGHT_TYPE == LIGHT_TYPE_SPOT
    const uint lightIdx = u_spotLightDraws[input.drawIndex].localLightIdx;
#endif // LIGHT_TYPE

    const uint vertexIdx = input.index;

    const LocalLightInterface localLight = u_localLights[lightIdx];

#if LIGHT_TYPE == LIGHT_TYPE_POINT
    const float3 lightProxyVertLocation = localLight.location + u_pointLightProxyVertices[vertexIdx] * localLight.range;
#elif LIGHT_TYPE == LIGHT_TYPE_SPOT
	const float3 forward = localLight.direction;
	const float3 right = abs(localLight.direction.z) < 1.f - SMALL_NUMBER ? normalize(cross(forward, UP_VECTOR)) : RIGHT_VECTOR;
	const float3 up = cross(right, forward);

	const float forwardScaling = localLight.range;
	const float sideScaling = forwardScaling * localLight.halfAngleTan;
	const float3x3 lightToWorldMatrix = float3x3(
		forward * forwardScaling,
		right * sideScaling,
		up * sideScaling
	);
	const float3 localVertexPos = u_spotLightProxyVertices[vertexIdx];

	const float3 lightProxyVertLocation = localLight.location + mul(localVertexPos, lightToWorldMatrix);
#endif // LIGHT_TYPE

    output.clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(lightProxyVertLocation, 1.f));
    output.fragmentClipSpace = output.clipSpace;
    output.lightIdx = lightIdx;

    return output;
}


void BuildLightsTilesPS(VS_OUTPUT vertexInput)
{
    const float2 uv = vertexInput.fragmentClipSpace.xy / vertexInput.fragmentClipSpace.w * 0.5f + 0.5f;

    const uint2 tileCoords = GetLightsTile(uv, u_lightsData.tileSize);

    const uint tileLights32Offset = GetLightsTileDataOffsetForLight(tileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num, vertexInput.lightIdx);
    const uint lightMask = 1u << (vertexInput.lightIdx % 32);
    InterlockedOr(u_tilesLightsMask[tileLights32Offset], lightMask);
}
