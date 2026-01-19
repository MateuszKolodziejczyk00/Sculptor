#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PREFABASSET_AS_DLL
		#ifdef PREFABASSET_BUILD_DLL
			#define PREFAB_ASSET_API __declspec(dllexport)
		#else
			#define PREFAB_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define PREFAB_ASSET_API
	#endif // PREFABASSET_BUILD_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
