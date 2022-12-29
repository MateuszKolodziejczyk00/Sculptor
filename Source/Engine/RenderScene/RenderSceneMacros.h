#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERSCENE_BUILD_DLL
		#define RENDER_SCENE_API __declspec(dllexport)
	#else
		#define RENDER_SCENE_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
