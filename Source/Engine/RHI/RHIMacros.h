#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RHI_AS_DLL
		#ifdef RHI_BUILD_DLL
			#define RHI_API __declspec(dllexport)
		#else
			#define RHI_API __declspec(dllimport)
		#endif
	#else
		#define RHI_API
	#endif // RHI_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS