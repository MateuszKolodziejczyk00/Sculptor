#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PLATFORM_AS_DLL
		#ifdef PLATFORM_BUILD_DLL
			#define PLATFORM_API __declspec(dllexport)
		#else
			#define PLATFORM_API __declspec(dllimport)
		#endif
	#else
		#define PLATFORM_API
	#endif // PLATFORM_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
