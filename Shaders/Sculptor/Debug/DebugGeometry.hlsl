#include "SculptorShader.hlsli"

[[descriptor_set(DebugGeometryPassDS, 0)]]


#define DEBUG_GEOMETRY_TYPE_LINE 0


struct VS_INPUT
{
    uint instanceIdx    : SV_InstanceID;
    uint vertexIdx      : SV_VertexID;
};


struct VS_OUTPUT
{
    float4  clipSpace : SV_POSITION;
    float4  color     : COLOR;
};


VS_OUTPUT ProcessLine(VS_INPUT input)
{
    VS_OUTPUT output;

	const DebugLineDefinition lineDef = u_passConstants.lines.Load(input.instanceIdx);

	const float3 vertexWS = input.vertexIdx == 0u ? lineDef.begin : lineDef.end;

	output.clipSpace = mul(u_passConstants.viewProjectionMatrix, float4(vertexWS, 1.0f));
	output.color     = float4(lineDef.color, 1.f);

    return output;
}


VS_OUTPUT DebugGeometryVS(VS_INPUT input)
{
#if DEBUG_GEOMETRY_TYPE == DEBUG_GEOMETRY_TYPE_LINE
    return ProcessLine(input);
#endif // DEBUG_GEOMETRY_TYPE == DEBUG_GEOMETRY_TYPE_LINE
}


struct PS_OUTPUT
{
    float4  color : SV_TARGET0;
};


PS_OUTPUT DebugGeometryPS(VS_OUTPUT vertexInput)
{
    PS_OUTPUT output;
	output.color = vertexInput.color;
    return output;
}
