#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCULPTORDLSSVULKAN_BUILD_DLL
		#define SCULPTORDLSSVULKAN_API __declspec(dllexport)
	#else
		#define SCULPTORDLSSVULKAN_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
