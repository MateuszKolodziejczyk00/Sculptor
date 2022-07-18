#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RHICORE_BUILD_DLL
		#define RHICORE_API __declspec(dllexport)
	#else
		#define RHICORE_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS