#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedResource.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

using RGAllocator = lib::StackTrackingAllocator<RGTrackedResource>;

} // spt::rg