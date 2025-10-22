#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERERCORE_AS_DLL
		#ifdef RENDERERCORE_BUILD_DLL
			#define RENDERER_CORE_API __declspec(dllexport)
		#else
			#define RENDERER_CORE_API __declspec(dllimport)
		#endif
	#else
		#define RENDERER_CORE_API
	#endif // RENDERERCORE_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS