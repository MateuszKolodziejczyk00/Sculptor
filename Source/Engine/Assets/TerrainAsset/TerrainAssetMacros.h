#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef TERRAINASSET_AS_DLL
		#ifdef TERRAINASSET_BUILD_DLL
			#define TERRAIN_ASSET_API __declspec(dllexport)
		#else
			#define TERRAIN_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define TERRAIN_ASSET_API
	#endif // TERRAINASSET_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
