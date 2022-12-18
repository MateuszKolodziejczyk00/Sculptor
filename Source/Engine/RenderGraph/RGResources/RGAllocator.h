#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

using RGAllocator = lib::StackTrackingAllocator<RGTrackedObject>;

} // spt::rg