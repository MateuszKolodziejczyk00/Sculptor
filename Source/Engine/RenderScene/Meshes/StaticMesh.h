#pragma once

#include "RHICore/RHIBufferTypes.h"

namespace spt::rsc
{

struct StaticMeshGeometryData
{
	Uint32 firstPrimitiveIdx;
	Uint32 primitivesNum;
	rhi::RHISuballocation primtivesSuballocation;
	rhi::RHISuballocation geometrySuballocation;
};

struct alignas(16) StaticMeshInstance
{
	math::Transform3f transform;
	Uint32 firstPrimitiveIdx;
	Uint32 primitivesNum;
};

} // spt::rsc
