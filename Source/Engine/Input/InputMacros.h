#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef INPUT_AS_DLL
		#ifdef INPUT_BUILD_DLL
			#define INPUT_API __declspec(dllexport)
		#else
			#define INPUT_API __declspec(dllimport)
		#endif
	#else
		#define INPUT_API
	#endif // INPUT_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
