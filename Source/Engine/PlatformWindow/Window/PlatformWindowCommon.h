#pragma once

#include "SculptorCoreTypes.h"


namespace spt::platf
{

struct RequiredExtensionsInfo
{
	RequiredExtensionsInfo()
		: extensions(nullptr)
		, extensionsNum(0)
	{ }

	const char* const*					extensions;
	Uint32								extensionsNum;

};

}