#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PROFILER_BUILD_DLL
		#define PROFILER_API __declspec(dllexport)
	#else
		#define PROFILER_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
