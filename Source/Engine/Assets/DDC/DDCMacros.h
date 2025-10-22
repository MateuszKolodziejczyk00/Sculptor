#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef DDC_AS_DLL
		#ifdef DDC_BUILD_DLL
			#define DDC_API __declspec(dllexport)
		#else
			#define DDC_API __declspec(dllimport)
		#endif
	#else
		#define DDC_API
	#endif // DDC_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
