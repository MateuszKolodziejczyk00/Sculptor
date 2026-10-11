#include "SculptorShader.hlsli"

[[shader_params(ClosureViewerData, VIEWER)]]

#include "Shading/Shading.hlsli"
#include "Tools/ClosureViewer/ClosureViewerCommon.hlsli"


struct VS_INPUT
{
	uint vertexIdx : SV_VertexID;
};


struct PS_OUTPUT
{
	float4 color : SV_TARGET0;
};


float4 ProjectWorldLocation(in float3 location)
{
	return mul(VIEWER->viewProjectionMatrix, float4(location, 1.f));
}

// Ground Grid ===========================================================================

static const float groundGridExtent   = 2.f;
static const float groundGridCellSize = 0.25f;


struct GROUND_GRID_VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float2 location  : LOCATION;
};


GROUND_GRID_VS_OUTPUT GroundGridVS(VS_INPUT input)
{
	const float2 corners[6] =
	{
		float2(-1.f, -1.f), float2(1.f, -1.f), float2(1.f, 1.f),
		float2(-1.f, -1.f), float2(1.f, 1.f),  float2(-1.f, 1.f)
	};

	const float2 location = corners[input.vertexIdx] * groundGridExtent;

	GROUND_GRID_VS_OUTPUT output;
	output.clipSpace = ProjectWorldLocation(float3(location, 0.f));
	output.location  = location;
	return output;
}


PS_OUTPUT GroundGridFS(GROUND_GRID_VS_OUTPUT vertexInput)
{
	const float2 cellCoords = vertexInput.location / groundGridCellSize;
	const float2 lineDist   = abs(frac(cellCoords - 0.5f) - 0.5f) / fwidth(cellCoords);
	const float lineFactor  = 1.f - saturate(min(lineDist.x, lineDist.y));

	const float2 axisDist   = abs(vertexInput.location) / fwidth(vertexInput.location);
	const float axisFactor  = 1.f - saturate(min(axisDist.x, axisDist.y) * 0.5f);

	const float fade = 1.f - saturate(length(vertexInput.location) / groundGridExtent);

	float3 color = 0.1f;
	color = lerp(color, 0.35f, lineFactor * fade);
	color = lerp(color, 0.6f, axisFactor * fade);

	PS_OUTPUT output;
	output.color = float4(color, 1.f);
	return output;
}

// Direction Lines =======================================================================

static const float directionLineLength = 1.5f;


struct DIRECTION_LINES_VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float3 color     : COLOR;
};


DIRECTION_LINES_VS_OUTPUT DirectionLinesVS(VS_INPUT input)
{
	const uint lineIdx   = input.vertexIdx / 2u;
	const bool isLineEnd = (input.vertexIdx & 1u) != 0u;

	const float3 N = UP_VECTOR;
	const float3 V = VIEWER->viewDirection;
	const float3 L = reflect(-V, N);

	const float3 directions[3] = { V, L, N };
	const float3 colors[3] =
	{
		float3(1.f, 0.85f, 0.1f),
		float3(1.f, 0.3f, 0.3f),
		float3(0.3f, 0.6f, 1.f)
	};

	const float3 location = isLineEnd ? directions[lineIdx] * directionLineLength : 0.f;

	DIRECTION_LINES_VS_OUTPUT output;
	output.clipSpace = ProjectWorldLocation(location);
	output.color     = colors[lineIdx];
	return output;
}


PS_OUTPUT DirectionLinesFS(DIRECTION_LINES_VS_OUTPUT vertexInput)
{
	PS_OUTPUT output;
	output.color = float4(vertexInput.color, 1.f);
	return output;
}

// Closure ===============================================================================

struct CLOSURE_VS_OUTPUT
{
	float4 clipSpace : SV_POSITION;
	float3 location  : LOCATION;
	float3 color     : COLOR;
};


CLOSURE_VS_OUTPUT Closure3DVS(VS_INPUT input)
{
	const float3 L = VIEWER->closure3DVertices[input.vertexIdx];
	const float3 N = UP_VECTOR;
	const float3 V = VIEWER->viewDirection;

	const ShadedSurface surface = CreateViewerSurface(N);
	const BRDFLobes lobes = OpaqueClosure::Evaluate<false>(surface, V, L);

	const float diffuseLum  = Luminance(lobes.diffuse);
	const float specularLum = Luminance(lobes.specular);
	const float totalLum    = diffuseLum + specularLum;

	const float3 location = L * totalLum * VIEWER->closureScale;

	const float3 diffuseColor  = float3(0.2f, 0.55f, 1.f);
	const float3 specularColor = float3(1.f, 0.55f, 0.15f);

	CLOSURE_VS_OUTPUT output;
	output.clipSpace = ProjectWorldLocation(location);
	output.location  = location;
	output.color     = lerp(diffuseColor, specularColor, totalLum > 0.f ? specularLum / totalLum : 0.f);
	return output;
}


PS_OUTPUT Closure3DFS(CLOSURE_VS_OUTPUT vertexInput)
{
	const float3 faceNormal = normalize(cross(ddx(vertexInput.location), ddy(vertexInput.location)));
	const float3 lightDir   = normalize(float3(-0.3f, -0.6f, 0.75f));
	const float shading     = 0.35f + 0.65f * abs(dot(faceNormal, lightDir));

	PS_OUTPUT output;
	output.color = float4(vertexInput.color * shading, VIEWER->closureOpacity);
	return output;
}
