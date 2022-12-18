#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

class RENDER_GRAPH_API RGTrackedObject
{
public:

	RGTrackedObject() = default;
	virtual ~RGTrackedObject() = default;
};

} // spt::rg
