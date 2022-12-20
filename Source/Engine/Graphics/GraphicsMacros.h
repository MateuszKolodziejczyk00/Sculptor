#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef GRAPHICS_BUILD_DLL
		#define GRAPHICS_API __declspec(dllexport)
	#else
		#define GRAPHICS_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
