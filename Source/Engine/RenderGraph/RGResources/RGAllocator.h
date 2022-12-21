#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

class RENDER_GRAPH_API RGStackMemoryChunksAllocator
{
public:

	RGStackMemoryChunksAllocator() = default;

	Byte* Allocate(SizeType size);
	void Deallocate(Byte* ptr, SizeType size);

};

constexpr SizeType RenderGraphMemoryChunkSize = 1024 * 1024;

using RGAllocator = lib::StackTrackingAllocator<RGTrackedObject, RGStackMemoryChunksAllocator>;

} // spt::rg