#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef MATERIALASSET_AS_DLL
		#ifdef MATERIALASSET_BUILD_DLL
			#define MATERIAL_ASSET_API __declspec(dllexport)
		#else
			#define MATERIAL_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define MATERIAL_ASSET_API
	#endif // MATERIALASSET_BUILD_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
