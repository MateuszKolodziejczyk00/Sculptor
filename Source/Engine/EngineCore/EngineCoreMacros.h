#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef ENGINECORE_BUILD_DLL
		#define ENGINE_CORE_API __declspec(dllexport)
	#else
		#define ENGINE_CORE_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
