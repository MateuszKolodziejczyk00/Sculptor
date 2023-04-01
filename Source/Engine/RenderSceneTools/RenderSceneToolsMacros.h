#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERSCENETOOLS_BUILD_DLL
		#define RENDER_SCENE_TOOLS_API __declspec(dllexport)
	#else
		#define RENDER_SCENE_TOOLS_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
