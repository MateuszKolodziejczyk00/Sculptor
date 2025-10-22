#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef UICORE_AS_DLL
		#ifdef UICORE_BUILD_DLL
			#define UI_CORE_API __declspec(dllexport)
		#else
			#define UI_CORE_API __declspec(dllimport)
		#endif
	#else
		#define UI_CORE_API
	#endif // UICORE_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
