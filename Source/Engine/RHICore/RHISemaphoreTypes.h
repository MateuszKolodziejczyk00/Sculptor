#pragma once


namespace spt::rhi
{

enum class ESemaphoreType
{
	Binary,
	Timeline
};


struct SemaphoreDefinition
{
	SemaphoreDefinition(ESemaphoreType type, Uint64 initialValue = 0)
		: m_type(type)
		, m_initialValue(initialValue)
	{ }

	ESemaphoreType			m_type;
	Uint64						m_initialValue;
};

}