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


namespace EPipelineStage
{

enum Flags : Flags64
{
	TopOfPipe						= BIT64(0),
	DrawIndirect					= BIT64(1),
	VertexShader					= BIT64(2),
	FragmentShader					= BIT64(3),
	EarlyFragmentTest				= BIT64(4),
	LateFragmentTest				= BIT64(5),
	ColorRTOutput					= BIT64(6),
	ComputeShader					= BIT64(7),
	Transfer						= BIT64(8),
	BottomOfPipe					= BIT64(9),
	Copy							= BIT64(10),
	Blit							= BIT64(11),
	Clear							= BIT64(12),
	IndexInput						= BIT64(13),

	ALL_GRAPHICS					= BIT64(50),
	ALL_TRANSFER					= BIT64(51),
	ALL_COMMANDS					= BIT64(52)
};

}

}