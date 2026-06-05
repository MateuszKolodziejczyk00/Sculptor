#ifndef GRASS_GEOMETRY_H
#define GRASS_GEOMETRY_H

#include "Terrain/SceneTerrain.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/BezierCurves.hlsli"
#include "Terrain/Grass/GrassTypes.hlsli"


#define GRASS_BLADE_FLAGS_NONE  0u
#define GRASS_BLADE_FLAGS_LOD_1 1u


#define GRASS_BLADE_VERTICES_NUM_LOD0  13u
#define GRASS_BLADE_TRIANGLES_NUM_LOD0 11u

#define GRASS_BLADE_VERTICES_NUM_LOD1  7u
#define GRASS_BLADE_TRIANGLES_NUM_LOD1 5u

#define GRASS_WIDTH 0.03


static const float2 g_grassBladeVertices_LOD0[GRASS_BLADE_VERTICES_NUM_LOD0] =
{
	float2(-GRASS_WIDTH * 1.0f, 0.0f),
	float2( GRASS_WIDTH * 1.0f, 0.0f),
	float2(-GRASS_WIDTH * 0.92f, 0.18f),
	float2( GRASS_WIDTH * 0.92f, 0.18f),
	float2(-GRASS_WIDTH * 0.8f, 0.36f),
	float2( GRASS_WIDTH * 0.8f, 0.36f),
	float2(-GRASS_WIDTH * 0.62f, 0.54f),
	float2( GRASS_WIDTH * 0.62f, 0.54f),
	float2(-GRASS_WIDTH * 0.42f, 0.72f),
	float2( GRASS_WIDTH * 0.42f, 0.72f),
	float2(-GRASS_WIDTH * 0.22f, 0.88f),
	float2( GRASS_WIDTH * 0.22f, 0.88f),
	float2( 0.0f, 1.0f)
};


static const float2 g_grassBladeVertices_LOD1[GRASS_BLADE_VERTICES_NUM_LOD1] =
{
	float2(-GRASS_WIDTH * 1.0f, 0.0f),
	float2( GRASS_WIDTH * 1.0f, 0.0f),
	float2(-GRASS_WIDTH * 0.8f, 0.36f),
	float2( GRASS_WIDTH * 0.8f, 0.36f),
	float2(-GRASS_WIDTH * 0.42f, 0.72f),
	float2( GRASS_WIDTH * 0.42f, 0.72f),
	float2( 0.0f, 1.0f)
};


#if defined(GRASS_BLADES_LOD)
#define g_grassBladeVertices  g_grassBladeVertices_LOD##GRASS_BLADES_LOD
#define g_grassBladeTriangles g_grassBladeTriangles_LOD##GRASS_BLADES_LOD

#if GRASS_BLADES_LOD == 0
#define GRASS_BLADE_VERTICES_NUM  GRASS_BLADE_VERTICES_NUM_LOD0
#define GRASS_BLADE_TRIANGLES_NUM GRASS_BLADE_TRIANGLES_NUM_LOD0
#elif GRASS_BLADES_LOD == 1
#define GRASS_BLADE_VERTICES_NUM  GRASS_BLADE_VERTICES_NUM_LOD1
#define GRASS_BLADE_TRIANGLES_NUM GRASS_BLADE_TRIANGLES_NUM_LOD1
#endif
#endif // GRASS_BLADES_LOD


uint GenerateGrassBladeHash(in float2 location)
{
	const uint x = asuint(location.x * 1000.f);
	const uint y = asuint(location.y * 1000.f);
	return x * 73856093u ^ y * 19349663u;
}


float GrassBladeRandom(in GrassBladeDef bladeDef)
{
	return PCGHash(GenerateGrassBladeHash(bladeDef.location)) / 4294967296.f;
}


uint3 LoadGrassBladeTriangleIndices(in uint triangleIdx)
{
	const uint a = (triangleIdx + 1u) & ~1u;
	const uint b = triangleIdx + 2u;
	const uint c = (triangleIdx & ~1u) + 1u;

	return uint3(a, b, c);
}


struct GrassBladeVertex
{
	float3 location;
	float3 normal;
	float  heightAlpha;
};


[[shader_struct(GrassBladeDef)]]


int2 ReconstructClumpCoord(in GrassBladeDef bladeDef)
{
	const int2 clumpCoord8 = int2(bladeDef.clumpCoords & 0xFFu, (bladeDef.clumpCoords >> 8u) & 0xFFu);

	const float clumpDist = 2.f;
	const float repeatRange = 256.f * clumpDist;
	const float2 clumpLocalCenter = float2(clumpCoord8) * clumpDist;
	const int2 repeats = int2(round((bladeDef.location - clumpLocalCenter) / repeatRange));

	return clumpCoord8 + repeats * 256;
}

float2 ReconstructClumpCenter(in GrassBladeDef bladeDef)
{
	return float2(ReconstructClumpCoord(bladeDef)) * 2.f;
}

float2 ReconstructClumpLocation(in GrassBladeDef bladeDef)
{
	const int2 clumpCoord = ReconstructClumpCoord(bladeDef);
	const uint2 clumpCoord8 = uint2(clumpCoord) & 255u;

	RngState rng = RngState::Create(clumpCoord8, 0u);

	const float clumpDist = 2.f;
	const float2 offset = (float2(rng.Next(), rng.Next()) - 0.5f) * clumpDist;

	return ReconstructClumpCenter(bladeDef) + offset;
}

struct GrassVertexProcessor
{
	float3        origin;
	float         yaw;
	float         height;
	float2        normal;
	float2        sideDirection;
	float         leanYaw;
	float         leanFactor;
	float         materialVariation;
	bool          isLOD1;

	RngState      bladeRng;
	RngState      clumpRng;

	static GrassVertexProcessor Create(GrassBladeDef inBladeDef)
	{
		TerrainInterface terrain = SceneTerrain();

		RngState bladeRng;
		bladeRng.seed = GenerateGrassBladeHash(inBladeDef.location);

		const float2 clumpCenter = ReconstructClumpCenter(inBladeDef);

		RngState clumpRng;
		clumpRng.seed = GenerateGrassBladeHash(clumpCenter);

		GrassVertexProcessor processor;

		const float originZ = terrain.GetHeight(inBladeDef.location);
		processor.origin = float3(clumpCenter + (inBladeDef.location - clumpCenter) * 1.0f, originZ);

		processor.yaw               = (clumpRng.Next() + 0.5f * bladeRng.Next()) * 6.28318530718f;
		processor.height            = 0.5f + (clumpRng.Next() + 0.3f * bladeRng.Next()) * 0.7f;
		processor.normal            = float2(sin(processor.yaw), cos(processor.yaw));
		processor.sideDirection     = processor.normal.yx;
		processor.leanYaw           = (clumpRng.Next() + bladeRng.Next() * 0.6f) * 6.28318530718f;
		processor.leanFactor        = (clumpRng.Next() * 0.7f + bladeRng.Next() * 0.3f) * 0.5f + 0.5f;
		processor.materialVariation = (clumpRng.Next() * 0.7f + bladeRng.Next() * 0.3f);
		processor.isLOD1            = (inBladeDef.flags & GRASS_BLADE_FLAGS_LOD_1) != 0u;
		processor.bladeRng          = bladeRng;
		processor.clumpRng          = clumpRng;

		return processor;
	}

	float3 GetBladeOrigin()
	{
		return origin;
	}

	float3 GetBladeNormal()
	{
		return float3(normal, 0.f);
	}

	float3 GetBladeTangent()
	{
		return float3(sideDirection, 0.f);
	}

	float3 GetBladeBitangent()
	{
		return float3(0.f, 0.f, 1.f);
	}

	float3 GetVertexLocation(in uint bladeVertexIdx)
	{
		return GetBladeOrigin() + GetVertexLocalLocation(bladeVertexIdx);
	}

	float3 GetVertexLocalLocation(in uint bladeVertexIdx, out float3 outNormal)
	{
		const float3 bladeOrigin = GetBladeOrigin();

		const float3 viewDir = normalize(u_sceneView.viewLocation - bladeOrigin);
		const float dotVN = abs(dot(viewDir, GetBladeNormal()));
		const float thickeningFactor = 1.f / max(dotVN, 0.8f);

		const float2 bladeVertex = isLOD1 ? g_grassBladeVertices_LOD1[bladeVertexIdx] : g_grassBladeVertices_LOD0[bladeVertexIdx];

		const float3 leanDir = float3(cos(leanYaw), sin(leanYaw), 0.f);
		const float3 startPoint = 0.f;
		const float3 endPoint   = float3(0.f, 0.f, height * (1.f - leanFactor * 0.5f)) + leanDir * height * leanFactor;

		const float3 p1 = lerp(startPoint, endPoint, 0.3f) + float3(0.f, 0.f, 0.35f);
		const float3 p2 = lerp(startPoint, endPoint, 0.6f) + float3(0.f, 0.f, 0.15f);

		Bezier::Cubic::Curve<float3> curve;
		curve.p0 = startPoint;
		curve.p1 = p1;
		curve.p2 = p2;
		curve.p3 = endPoint;

		const float3 pointOnCurve = curve.Evaluate(bladeVertex.y);
		const float3 tangent = normalize(curve.EvaluateDerivative(bladeVertex.y));

		float width = bladeVertex.x * thickeningFactor;
		if ((isLOD1))
		{
			width *= 3.f;
		}

		float3 vert = pointOnCurve;
		vert.xy += sideDirection * width;

		outNormal = normalize(cross(tangent, float3(sideDirection, 0.f)));

		return vert;
	}

	float3 GetVertexLocalLocation(in uint bladeVertexIdx)
	{
		float3 normal;
		return GetVertexLocalLocation(bladeVertexIdx, normal);
	}

	GrassBladeVertex ProcessVertex(in uint bladeVertexIdx)
	{
		const float3 bladeOrigin = GetBladeOrigin();

		float3 normal;
		const float3 localLocation = GetVertexLocalLocation(bladeVertexIdx, OUT normal);

		float widthAlpha = isLOD1 ? g_grassBladeVertices_LOD1[bladeVertexIdx].x : g_grassBladeVertices_LOD0[bladeVertexIdx].x;
		widthAlpha /= GRASS_WIDTH;

		const float3 shadingNormal = normalize(normal + float3(sideDirection, 0.f) * widthAlpha * 0.5f);

		GrassBladeVertex vertex;
		vertex.location    = bladeOrigin + localLocation;
		vertex.normal      = shadingNormal;
		vertex.heightAlpha = localLocation.y / height;
		return vertex;
	}
};


void ProcessGrassBladeTriangleVertices(in GrassVertexProcessor processor, in uint triangleIdx, out GrassBladeVertex outVertex0, out GrassBladeVertex outVertex1, out GrassBladeVertex outVertex2)
{
	const uint3 triangleIndices = LoadGrassBladeTriangleIndices(triangleIdx);

	outVertex0 = processor.ProcessVertex(triangleIndices.x);
	outVertex1 = processor.ProcessVertex(triangleIndices.y);
	outVertex2 = processor.ProcessVertex(triangleIndices.z);
}

#endif // GRASS_GEOMETRY_H
