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

	static void		Initialize();

	static void		FlipFrame(const RHIWindow& window);
};

} // spt::vulkan