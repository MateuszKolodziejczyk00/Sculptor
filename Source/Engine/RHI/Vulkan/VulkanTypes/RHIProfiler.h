#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class RHIWindow;
class RHICommandBuffer;


class RHI_API RHIProfiler
{
public:

	// Generic context type - might be simple handle or int, or pointer to more complex data structure
	using CmdBufferContext	= Uint64;

	static void				Initialize();

	static void				FlipFrame(const RHIWindow& window);

	static CmdBufferContext	GetCommandBufferContext(const RHICommandBuffer& cmdBuffer);
};

} // spt::vulkan