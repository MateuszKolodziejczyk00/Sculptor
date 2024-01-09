#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(DDGIDebugDrawRaysDS, 1)]]

#include "DDGI/DDGITypes.hlsli"


struct VS_INPUT
{
    uint instanceIdx    : SV_InstanceID;
    uint vertexIdx      : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace   : SV_POSITION;
    uint    rayIdx      : RAY_IDX;
};


VS_OUTPUT DDGIDebugRaysVS(VS_INPUT input)
{
    VS_OUTPUT output;

    const DDGIDebugRay ray = u_debugRays[input.instanceIdx];

    const float3 vertexWS = input.vertexIdx == 0 ? ray.traceOrigin : ray.traceEnd;

    output.clipSpace  = mul(u_sceneView.viewProjectionMatrixNoJitter, float4(vertexWS, 1.0f));
    output.rayIdx     = input.instanceIdx;

    return output;
}


struct PS_OUTPUT
{
    float4  color : SV_TARGET0;
};


PS_OUTPUT DDGIDebugRaysPS(VS_OUTPUT vertexInput)
{
    PS_OUTPUT output;

    const DDGIDebugRay ray = u_debugRays[vertexInput.rayIdx];

    float3 color = ray.luminance;
    color /= (color + 1.0f);
    output.color = float4(color, 1.0f);
    
    // gamma
    output.color = pow(output.color, 0.4545);
    
    return output;
}
