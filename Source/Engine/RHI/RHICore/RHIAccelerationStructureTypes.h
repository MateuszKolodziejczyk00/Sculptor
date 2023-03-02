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


enum class ETLASInstanceFlags
{
	None			= 0,
	ForceOpaque		= BIT(0),
	ForceNoOpaque	= BIT(1),

	Default			= None
};


struct TLASInstanceDefinition
{
	using TransformMatrix = math::Matrix<Real32, 3, 4>;

	TLASInstanceDefinition()
		: transform(TransformMatrix::Identity())
		, customIdx(0)
		, mask(0xFF)
		, sbtRecordOffset(0)
		, flags(ETLASInstanceFlags::Default)
		, blasAddress(0)
	{ }

	TransformMatrix		transform;
	Uint32				customIdx;
	Uint32				mask : 8;
	Uint32				sbtRecordOffset : 24;
	ETLASInstanceFlags	flags;
	Uint64				blasAddress;
};


struct TLASDefinition
{
	TLASDefinition()
	{ }

	lib::DynamicArray<TLASInstanceDefinition> instances;
};

} // spt::rhi