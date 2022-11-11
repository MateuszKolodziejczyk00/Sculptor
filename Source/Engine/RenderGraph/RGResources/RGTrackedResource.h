#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

class RENDER_GRAPH_API RGTrackedResource
{
public:

	RGTrackedResource() = default;
	virtual ~RGTrackedResource() = default;
};

} // spt::rg
