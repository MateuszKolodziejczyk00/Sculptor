#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERSCENETOOLS_AS_DLL
		#ifdef RENDERSCENETOOLS_BUILD_DLL
			#define RENDER_SCENE_TOOLS_API __declspec(dllexport)
		#else
			#define RENDER_SCENE_TOOLS_API __declspec(dllimport)
		#endif
	#else
		#define RENDER_SCENE_TOOLS_API
	#endif // RENDERSCENETOOLS_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
