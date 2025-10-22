#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef ASSETSSYSTEM_AS_DLL
		#ifdef ASSETSSYSTEM_BUILD_DLL
			#define ASSETS_SYSTEM_API __declspec(dllexport)
		#else
			#define ASSETS_SYSTEM_API __declspec(dllimport)
		#endif
	#else
		#define ASSETS_SYSTEM_API
	#endif // ASSETSSYSTEM_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
