#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERERCORE_BUILD_DLL
		#define RENDERER_CORE_API __declspec(dllexport)
	#else
		#define RENDERER_CORE_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS