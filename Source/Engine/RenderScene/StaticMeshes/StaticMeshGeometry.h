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

} // spt::rsc
