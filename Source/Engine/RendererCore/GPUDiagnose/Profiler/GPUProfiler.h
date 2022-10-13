#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIProfilerImpl.h"
#include "GPUProfiler.h"


namespace spt::rdr
{

class Window;


class RENDERER_CORE_API GPUProfiler
{
public:

	static void						Initialize();

	static void						FlipFrame(const lib::SharedPtr<Window>& window);
};

} // spt::rdr


#if PROFILE_GPU

#if WITH_OPTICK

#define SPT_GPU_PROFILER_CONTEXT(cmdBuffer)	OPTICK_GPU_CONTEXT(cmdBuffer)
#define SPT_GPU_PROFILER_EVENT(Name)		OPTICK_GPU_EVENT(Name)

#endif // WITH_OPTICK

#else

#define SPT_GPU_PROFILER_CONTEXT(cmdBuffer)
#define SPT_GPU_PROFILER_EVENT(Name)

#endif // PROFILE_GPI
