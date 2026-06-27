#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef GAMEFRAMEWORK_AS_DLL
		#ifdef GAMEFRAMEWORK_BUILD_DLL
			#define GAME_FRAMEWORK_API __declspec(dllexport)
		#else
			#define GAME_FRAMEWORK_API __declspec(dllimport)
		#endif
	#else
		#define GAME_FRAMEWORK_API
	#endif // GAMEFRAMEWORK_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
