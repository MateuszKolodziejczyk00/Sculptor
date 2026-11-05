#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef PROPERTIESPANEL_AS_DLL
		#ifdef PROPERTIESPANEL_BUILD_DLL
			#define PROPERTIES_PANEL_API __declspec(dllexport)
		#else
			#define PROPERTIES_PANEL_API __declspec(dllimport)
		#endif
	#else
		#define PROPERTIES_PANEL_API
	#endif // PROPERTIESPANEL_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
