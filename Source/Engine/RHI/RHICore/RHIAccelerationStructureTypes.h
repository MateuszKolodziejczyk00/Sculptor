#pragma once

#include "SculptorCoreTypes.h"
#include "RHICommonTypes.h"


namespace spt::rhi
{

enum class EBLASGeometryType
{
	Triangles
};

enum class EGeometryFlags
{
	None	 = 0,
	Opaque	= BIT(0),

	Default	= None
};


struct BLASTrianglesGeometryDefinition
{
	BLASTrianglesGeometryDefinition()
		: vertexLocationsAddress(0)
		, vertexLocationsStride(0)
		, maxVerticesNum(0)
		, indicesAddress(0)
		, indicesNum(0)
	{ }

	Uint64 vertexLocationsAddress;
	Uint32 vertexLocationsStride;
	Uint32 maxVerticesNum;

	Uint32 indicesAddress;
	Uint32 indicesNum;
};


struct BLASDefinition
{
	BLASDefinition()
		: geometryType(EBLASGeometryType::Triangles)
		, geometryFlags(EGeometryFlags::Default)
	{ }

	EBLASGeometryType	geometryType;
	EGeometryFlags		geometryFlags;
	union
	{
		BLASTrianglesGeometryDefinition trianglesGeometry;
	};
};

} // spt::rhi