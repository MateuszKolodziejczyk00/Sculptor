#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef CLOUDSCAPEASSET_AS_DLL
		#ifdef CLOUDSCAPEASSET_BUILD_DLL
			#define CLOUDSCAPE_ASSET_API __declspec(dllexport)
		#else
			#define CLOUDSCAPE_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define CLOUDSCAPE_ASSET_API
	#endif // CLOUDSCAPEASSET_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
