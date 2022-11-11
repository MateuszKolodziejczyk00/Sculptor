#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rhi
{

struct RHIInitializationInfo
{
	RHIInitializationInfo()
		: extensions(nullptr)
		, extensionsNum(0)
	{ }

	const char* const*		extensions;
	Uint32					extensionsNum;
};

struct RHIWindowInitializationInfo
{
	RHIWindowInitializationInfo()
		: framebufferSize(idxNone<Uint32>, idxNone<Uint32>)
		, minImageCount(2)
		, enableVSync(true)
	{ }

	math::Vector2u			framebufferSize;
	Uint32					minImageCount;
	Bool					enableVSync;
};

} // spt::rhi
