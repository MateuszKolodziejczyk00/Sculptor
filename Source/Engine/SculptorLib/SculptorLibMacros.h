#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCULPTORLIB_BUILD_DLL
		#define SCULPTOR_LIB_API __declspec(dllexport)
	#else
		#define SCULPTOR_LIB_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
