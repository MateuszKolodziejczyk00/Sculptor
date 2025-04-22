#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef TEXTUREASSET_BUILD_DLL
		#define TEXTURE_ASSET_API __declspec(dllexport)
	#else
		#define TEXTURE_ASSET_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
