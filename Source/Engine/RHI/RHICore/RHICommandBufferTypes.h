#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class ECommandBufferType
{
	Primary,
	Secondary
};


enum class ECommandBufferQueueType
{
	Graphics,
	AsyncCompute,
	Transfer
};


namespace ECommandBufferComplexityClass
{

enum Type : Uint32
{
	Low,
	Default,
	High,

	NUM
};

}


struct CommandBufferDefinition
{
	CommandBufferDefinition()
		: m_queueType(ECommandBufferQueueType::Graphics)
		, m_cmdBufferType(ECommandBufferType::Primary)
	{ }

	CommandBufferDefinition(ECommandBufferQueueType queueType, ECommandBufferType cmdBufferType, ECommandBufferComplexityClass::Type complexityClass = ECommandBufferComplexityClass::Default)
		: m_queueType(queueType)
		, m_cmdBufferType(cmdBufferType)
		, m_complexityClass(complexityClass)
	{ }

	ECommandBufferQueueType					m_queueType;
	ECommandBufferType						m_cmdBufferType;
	ECommandBufferComplexityClass::Type		m_complexityClass;
};


enum class ECommandBufferBeginFlags : Flags32
{
	None							= 0,
	OneTimeSubmit					= BIT(0),
	ContinueRendering				= BIT(1)
};


struct CommandBufferUsageDefinition
{
	CommandBufferUsageDefinition()
		: m_beginFlags(ECommandBufferBeginFlags::None)
	{ }

	CommandBufferUsageDefinition(ECommandBufferBeginFlags beginFlags)
		: m_beginFlags(beginFlags)
	{ }

	ECommandBufferBeginFlags m_beginFlags;
};

} // spt::rhi