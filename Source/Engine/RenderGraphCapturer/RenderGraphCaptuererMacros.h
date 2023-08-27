#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RENDERGRAPHCAPTURER_BUILD_DLL
		#define RENDER_GRAPH_CAPTURER_API __declspec(dllexport)
	#else
		#define RENDER_GRAPH_CAPTURER_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS