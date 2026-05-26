#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef TERRAINEDITOR_AS_DLL
		#ifdef TERRAINEDITOR_BUILD_DLL
			#define TERRAIN_EDITOR_API __declspec(dllexport)
		#else
			#define TERRAIN_EDITOR_API __declspec(dllimport)
		#endif
	#else
		#define TERRAIN_EDITOR_API
	#endif // TERRAINEDITOR_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
