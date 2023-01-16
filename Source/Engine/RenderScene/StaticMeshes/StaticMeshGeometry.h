#pragma once

#include "RHICore/RHIBufferTypes.h"

namespace spt::rsc
{

struct StaticMeshGeometryData
{
	rhi::RHISuballocation primRenderDataSuballocation;
	rhi::RHISuballocation geometrySuballocation;
};

} // spt::rsc
