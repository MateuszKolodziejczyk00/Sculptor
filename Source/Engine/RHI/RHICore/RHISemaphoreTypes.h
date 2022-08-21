#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class ESemaphoreType
{
	Binary,
	Timeline
};


struct SemaphoreDefinition
{
	SemaphoreDefinition()
		: type(ESemaphoreType::Binary)
		, initialValue(0)
	{ }

	SemaphoreDefinition(ESemaphoreType inType, Uint64 inInitialValue = 0)
		: type(inType)
		, initialValue(inInitialValue)
	{ }

	ESemaphoreType			type;
	Uint64					initialValue;
};

}