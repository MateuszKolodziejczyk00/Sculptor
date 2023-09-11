#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef MATERIALS_BUILD_DLL
		#define MATERIALS_API __declspec(dllexport)
	#else
		#define MATERIALS_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
