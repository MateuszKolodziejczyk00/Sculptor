#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef UICORE_BUILD_DLL
		#define UI_CORE_API __declspec(dllexport)
	#else
		#define UI_CORE_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
