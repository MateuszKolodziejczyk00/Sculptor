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
	Uint32 maxPrimitivesNum = 0u;
	Uint32 maxVerticesNum   = 0u;
};


struct BLASDefinition
{
	BLASDefinition()
		: geometryType(EBLASGeometryType::Triangles)
		, geometryFlags(EGeometryFlags::Default)
	{ }

	EBLASGeometryType geometryType;
	EGeometryFlags    geometryFlags;
	union
	{
		BLASTrianglesGeometryDefinition trianglesGeometry;
	};
};


struct BLASTriangleBuildInfo
{
	Uint64 vertexLocationsAddress = 0u;
	Uint64 indicesAddress         = 0u;
	Uint32 vertexLocationsStride  = 0u;
	Uint32 primitivesNum          = 0u;
};


struct BLASBuildInfo
{
	BLASTriangleBuildInfo trianglesBuildInfo;
};


enum class ETLASInstanceFlags
{
	None              = 0,
	ForceOpaque       = BIT(0),
	ForceNoOpaque     = BIT(1),
	FacingCullDisable = BIT(2),

	Default = None
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
	Uint32 maxInstancesNum = 0u;
};


struct TLASBuildInfo
{
	Uint64 instancesAddress = 0u;
	Uint32 instancesNum     = 0u;
};

} // spt::rhi
