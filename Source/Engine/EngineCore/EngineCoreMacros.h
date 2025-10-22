#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef ENGINECORE_AS_DLL
		#ifdef ENGINECORE_BUILD_DLL
			#define ENGINE_CORE_API __declspec(dllexport)
		#else
			#define ENGINE_CORE_API __declspec(dllimport)
		#endif
	#else
		#define ENGINE_CORE_API
	#endif // ENGINECORE_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
