#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCUI_AS_DLL
		#ifdef SCUI_BUILD_DLL
			#define SCUI_API __declspec(dllexport)
		#else
			#define SCUI_API __declspec(dllimport)
		#endif
	#else
		#define SCUI_API
	#endif // SCUI_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
