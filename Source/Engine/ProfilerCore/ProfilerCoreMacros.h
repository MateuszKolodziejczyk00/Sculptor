#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PROFILERCORE_AS_DLL
		#ifdef PROFILERCORE_BUILD_DLL
			#define PROFILER_CORE_API __declspec(dllexport)
		#else
			#define PROFILER_CORE_API __declspec(dllimport)
		#endif
	#else
		#define PROFILER_CORE_API
	#endif // PROFILERCORE_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
