#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCULPTORECS_AS_DLL
		#ifdef SCULPTORECS_BUILD_DLL
			#define SCULPTOR_ECS_API __declspec(dllexport)
		#else
			#define SCULPTOR_ECS_API __declspec(dllimport)
		#endif
	#else
		#define SCULPTOR_ECS_API
	#endif // SCULPTORECS_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
