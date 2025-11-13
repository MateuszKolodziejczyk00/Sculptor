#include "SculptorShader.hlsli"

[[descriptor_set(DebugGeometryPassDS, 0)]]


#define DEBUG_GEOMETRY_TYPE_LINE   0
#define DEBUG_GEOMETRY_TYPE_MARKER 1
#define DEBUG_GEOMETRY_TYPE_SPHERE 2


struct VS_INPUT
{
	uint instanceIdx : SV_InstanceID;
	uint vertexIdx   : SV_VertexID;
};


struct VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float4 color     : COLOR;
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


VS_OUTPUT ProcessMarker(VS_INPUT input)
{
	VS_OUTPUT output;

	const DebugMarkerDefinition markerDef = u_passConstants.markers.Load(input.instanceIdx);

	float3 localSpace = 0.f;
	const uint axis = (input.vertexIdx >> 1u);
	localSpace[axis] = (input.vertexIdx & 1u) == 0u ? -0.5f : 0.5f;

	const float3 vertexWS = markerDef.location + localSpace * markerDef.size;

	output.clipSpace = mul(u_passConstants.viewProjectionMatrix, float4(vertexWS, 1.0f));
	output.color     = float4(markerDef.color, 1.f);

	return output;
}


VS_OUTPUT ProcessSphere(VS_INPUT input)
{
	VS_OUTPUT output;

	const DebugSphereDefinition sphereDef = u_passConstants.spheres.Load(input.instanceIdx);

	const float3 localSpace = u_passConstants.sphereVertices.Load(input.vertexIdx) * sphereDef.radius;

	const float3 vertexWS = sphereDef.center + localSpace;

	output.clipSpace = mul(u_passConstants.viewProjectionMatrix, float4(vertexWS, 1.0f));
	output.color     = float4(sphereDef.color, 1.f);

	return output;
}


VS_OUTPUT DebugGeometryVS(VS_INPUT input)
{
#if DEBUG_GEOMETRY_TYPE == DEBUG_GEOMETRY_TYPE_LINE
	return ProcessLine(input);
#elif DEBUG_GEOMETRY_TYPE == DEBUG_GEOMETRY_TYPE_MARKER
	return ProcessMarker(input);
#elif DEBUG_GEOMETRY_TYPE == DEBUG_GEOMETRY_TYPE_SPHERE
	return ProcessSphere(input);
#endif // DEBUG_GEOMETRY_TYPE
}


struct PS_OUTPUT
{
	float4 color : SV_TARGET0;
};


PS_OUTPUT DebugGeometryPS(VS_OUTPUT vertexInput)
{
	PS_OUTPUT output;
	output.color = vertexInput.color;
	return output;
}
