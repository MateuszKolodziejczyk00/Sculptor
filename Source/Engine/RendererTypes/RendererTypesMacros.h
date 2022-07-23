#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERERTYPES_BUILD_DLL
		#define RENDERER_TYPES_API __declspec(dllexport)
	#else
		#define RENDERER_TYPES_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS