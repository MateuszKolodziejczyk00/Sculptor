#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rhi
{

struct RHIModuleData {};


struct RHIInitializationInfo
{
	RHIInitializationInfo()
	{ }

	lib::DynamicArray<lib::StringView> extensions;
};


struct RHIWindowInitializationInfo
{
	RHIWindowInitializationInfo()
		: framebufferSize(idxNone<Uint32>, idxNone<Uint32>)
		, enableVSync(true)
	{ }

	math::Vector2u			framebufferSize;
	Bool					enableVSync;
};

} // spt::rhi
