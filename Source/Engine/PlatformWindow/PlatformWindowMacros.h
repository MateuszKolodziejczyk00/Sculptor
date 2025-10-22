#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PLATFORMWINDOW_AS_DLL
		#ifdef PLATFORMWINDOW_BUILD_DLL
			#define PLATFORM_WINDOW_API __declspec(dllexport)
		#else
			#define PLATFORM_WINDOW_API __declspec(dllimport)
		#endif
	#else
		#define PLATFORM_WINDOW_API
	#endif // PLATFORMWINDOW_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS

#pragma warning(disable : 4251) // 'Class' needs to have dll-interface to be used by clients of class
							    // Disabled as it generates lots of warnings when using header-only libraries