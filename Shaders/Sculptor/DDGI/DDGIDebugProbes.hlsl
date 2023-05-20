#include "SculptorShader.hlsli"
#include "DDGI/DDGIDebugProbesShape.hlsli"


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(DDGIDebugDrawProbesDS, 1)]]
[[descriptor_set(DDGIDS, 2)]]

#include "DDGI/DDGITypes.hlsli"


#define DDGI_DEBUG_MODE_IRRADIANCE 1
#define DDGI_DEBUG_MODE_HIT_DISTANCE 2


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

    const float3 vertexLocation = g_debugProbeVertices[vertexIdx] * u_ddgiProbesDebugParams.probeRadius;

    output.clipSpace    = mul(u_sceneView.viewProjectionMatrixNoJitter, float4(probeLocation + vertexLocation, 1.0f));
    output.normal       = normalize(vertexLocation);
    output.probeCoords  = probeCoords;

    return output;
}


struct PS_OUTPUT
{
    float4  color : SV_TARGET0;
};


PS_OUTPUT DDGIDebugProbesPS(VS_OUTPUT vertexInput)
{
    PS_OUTPUT output;

    const float2 octahedronUV = GetProbeOctCoords(normalize(vertexInput.normal));

    if(u_ddgiProbesDebugParams.debugMode == DDGI_DEBUG_MODE_IRRADIANCE)
    {
        float3 probeIrradiance = SampleProbeIrradiance(u_ddgiParams, u_probesIrradianceTexture, u_probesDataSampler, vertexInput.probeCoords, octahedronUV);

        probeIrradiance = pow(probeIrradiance, u_ddgiParams.probeIrradianceEncodingGamma);

        probeIrradiance *= 10.f;
    
        output.color = float4(probeIrradiance / (probeIrradiance + 1.f), 1.f);
    }
    else if (u_ddgiProbesDebugParams.debugMode == DDGI_DEBUG_MODE_HIT_DISTANCE)
    {
        float2 hitDistance = SampleProbeHitDistance(u_ddgiParams, u_probesHitDistanceTexture, u_probesDataSampler, vertexInput.probeCoords, octahedronUV);
        hitDistance.y = sqrt(hitDistance.y);
        hitDistance /= length(u_ddgiParams.probesSpacing);

        output.color = float4(hitDistance, 0.0f, 1.0f);
    }
    
    // gamma
    output.color = pow(output.color, 0.4545);
    
    return output;
}
