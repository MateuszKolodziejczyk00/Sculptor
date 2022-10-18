#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef JOBSYSTEM_BUILD_DLL
		#define JOB_SYSTEM_API __declspec(dllexport)
	#else
		#define JOB_SYSTEM_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
