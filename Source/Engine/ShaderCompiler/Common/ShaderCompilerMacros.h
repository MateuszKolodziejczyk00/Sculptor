#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SHADERCOMPILER_BUILD_DLL
		#define SHADER_COMPILER_API __declspec(dllexport)
	#else
		#define SHADER_COMPILER_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
