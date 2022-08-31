#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PLATFORM_BUILD_DLL
		#define PLATFORM_API __declspec(dllexport)
	#else
		#define PLATFORM_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
