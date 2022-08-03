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
		: m_type(ESemaphoreType::Binary)
		, m_initialValue(0)
	{ }

	SemaphoreDefinition(ESemaphoreType type, Uint64 initialValue = 0)
		: m_type(type)
		, m_initialValue(initialValue)
	{ }

	ESemaphoreType			m_type;
	Uint64					m_initialValue;
};

}