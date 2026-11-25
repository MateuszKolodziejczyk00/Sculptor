#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCENEASSET_AS_DLL
		#ifdef SCENEASSET_BUILD_DLL
			#define SCENE_ASSET_API __declspec(dllexport)
		#else
			#define SCENE_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define SCENE_ASSET_API
	#endif // SCENEASSET_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
