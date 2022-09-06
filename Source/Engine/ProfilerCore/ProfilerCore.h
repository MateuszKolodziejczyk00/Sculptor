#pragma once

#if ENABLE_PROFILER

#if WITH_OPTICK

#include "optick.h"

#define SPT_PROFILER_FRAME(Name)		OPTICK_FRAME(Name)
#define SPT_PROFILER_FUNCTION()			OPTICK_EVENT()
#define SPT_PROFILER_SCOPE(Name)		OPTICK_EVENT(Name)
#define SPT_PROFILER_THREAD(Name)		OPTICK_THREAD(Name)

#else

#error Only Optick profiler is supported

#endif // WITH_OPTICK

#else // ENABLE_PROFILER

#define SPT_PROFILER_FRAME(Name)
#define SPT_PROFILER_FUNCTION()
#define SPT_PROFILER_SCOPE(Name)	
#define SPT_PROFILER_THREAD(Name)

#endif // ENABLE_PROFILER