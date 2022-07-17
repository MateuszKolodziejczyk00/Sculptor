#pragma once

#if ENABLE_PROFILER

#if WITH_OPTICK

#include "optick.h"

#define SPT_PROFILE_FRAME(Name)			OPTICK_FRAME(Name)
#define SPT_PROFILE_FUNCTION()			OPTICK_EVENT()
#define SPT_PROFILE_SCOPE(Name)			OPTICK_EVENT(Name)
#define SPT_PROFILE_THREAD(Name)		OPTICK_THREAD(Name)

#else

#error Only Optick profiler is supported

#endif // WITH_OPTICK

#else // ENABLE_PROFILER

#define SPT_PROFILE_FRAME(Name)
#define SPT_PROFILE_FUNCTION()
#define SPT_PROFILE_SCOPE(Name)	
#define SPT_PROFILE_THREAD(Name)

#endif // ENABLE_PROFILER