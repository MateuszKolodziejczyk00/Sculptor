#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCENERENDERER_AS_DLL
		#ifdef SCENERENDERER_BUILD_DLL
			#define SCENE_RENDERER_API __declspec(dllexport)
		#else
			#define SCENE_RENDERER_API __declspec(dllimport)
		#endif
	#else
		#define SCENE_RENDERER_API
	#endif // SCENERENDERER_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
