#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef BLACKBOARD_BUILD_DLL
		#define BLACKBOARD_API __declspec(dllexport)
	#else
		#define BLACKBOARD_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
