#include "SculptorShader.hlsli"
#include "DDGI/DDGIDebugProbesShape.hlsli"


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(DDGIDebugDrawProbesDS, 1)]]

#include "DDGI/DDGITypes.hlsli"

struct VS_INPUT
{
    uint instanceIdx    : SV_InstanceID;
    uint vertexIdx      : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace           : SV_POSITION;
    
    float3  normal              : NORMAL;

    uint3   probeCoords         : PROBE_COORDS;
};


VS_OUTPUT DDGIDebugProbesVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const uint probeIdx = input.instanceIdx;
    const uint vertexIdx = input.vertexIdx;

    const uint3 probesVolumeRes = u_ddgiParams.probesVolumeResolution;

    const uint probesPerPlane = probesVolumeRes.x * probesVolumeRes.y;

    const uint z = probeIdx / probesPerPlane;
    const uint y = (probeIdx - z * probesPerPlane) / probesVolumeRes.x;
    const uint x = probeIdx - z * probesPerPlane - y * probesVolumeRes.x;

    const uint3 probeCoords = uint3(x, y, z);

    const float3 probeLocation = GetProbeWorldLocation(u_ddgiParams, probeCoords);

    const float radius = 0.05f;
    const float3 vertexLocation = g_debugProbeVertices[vertexIdx] * radius;

    output.clipSpace    = mul(u_sceneView.viewProjectionMatrix, float4(probeLocation + vertexLocation, 1.0f));
    output.normal       = normalize(vertexLocation);
    output.probeCoords  = probeCoords;

    return output;
}


struct PS_OUTPUT
{
    float3  color : SV_TARGET0;
};


PS_OUTPUT DDGIDebugProbesPS(VS_OUTPUT vertexInput)
{
    const float2 octahedronUV = GetProbeOctCoords(normalize(vertexInput.normal));

    const float3 probeIrradiance = SampleProbeIrradiance(u_ddgiParams, u_probesIrradianceTexture, u_probesDataSampler, vertexInput.probeCoords, octahedronUV);

    PS_OUTPUT output;
    output.color = probeIrradiance;
    
    return output;
}
