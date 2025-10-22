#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef GRAPHICS_AS_DLL
		#ifdef GRAPHICS_BUILD_DLL
			#define GRAPHICS_API __declspec(dllexport)
		#else
			#define GRAPHICS_API __declspec(dllimport)
		#endif
	#else
		#define GRAPHICS_API
	#endif // GRAPHICS_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
