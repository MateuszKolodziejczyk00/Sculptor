#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef CLOUDSCAPEEDITOR_AS_DLL
		#ifdef CLOUDSCAPEEDITOR_BUILD_DLL
			#define CLOUDSCAPE_EDITOR_API __declspec(dllexport)
		#else
			#define CLOUDSCAPE_EDITOR_API __declspec(dllimport)
		#endif
	#else
		#define CLOUDSCAPE_EDITOR_API
	#endif // CLOUDSCAPEEDITOR_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
