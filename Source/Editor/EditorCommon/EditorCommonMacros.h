#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef EDITORCOMMON_AS_DLL
		#ifdef EDITORCOMMON_BUILD_DLL
			#define EDITOR_COMMON_API __declspec(dllexport)
		#else
			#define EDITOR_COMMON_API __declspec(dllimport)
		#endif
	#else
		#define EDITOR_COMMON_API
	#endif // EDITORCOMMON_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
