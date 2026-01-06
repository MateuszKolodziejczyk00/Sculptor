#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef MESHASSET_AS_DLL
		#ifdef MESHASSET_BUILD_DLL
			#define MESH_ASSET_API __declspec(dllexport)
		#else
			#define MESH_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define MESH_ASSET_API
	#endif // MESHASSET_BUILD_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
