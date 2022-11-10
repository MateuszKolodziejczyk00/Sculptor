#pragma once

#include "SculptorCoreTypes.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

class RGTrackedResource
{
public:

	RGTrackedResource() = default;
	virtual ~RGTrackedResource() = default;
};


using RenderGraphAllocator = lib::StackTrackingAllocator<RGTrackedResource>;

} // spt::rg