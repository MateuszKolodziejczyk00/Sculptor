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


enum class ECommandBufferComplexityClass
{
	Low,
	Default,
	High
};


struct CommandBufferDefinition
{
	CommandBufferDefinition()
		: m_queueType(ECommandBufferQueueType::Graphics)
		, m_cmdBufferType(ECommandBufferType::Primary)
	{ }

	CommandBufferDefinition(ECommandBufferQueueType queueType, ECommandBufferType cmdBufferType, ECommandBufferComplexityClass complexityClass = ECommandBufferComplexityClass::Default)
		: m_queueType(queueType)
		, m_cmdBufferType(cmdBufferType)
		, m_complexityClass(complexityClass)
	{ }

	ECommandBufferQueueType			m_queueType;
	ECommandBufferType				m_cmdBufferType;
	ECommandBufferComplexityClass	m_complexityClass;
};


namespace ECommandBufferBeginFlags
{

enum : Flags32
{
	OneTimeSubmit					= BIT(0),
	ContinueRendering				= BIT(1)
};

}


struct CommandBufferUsageDefinition
{
	CommandBufferUsageDefinition()
		: m_beginFlags(0)
	{ }

	CommandBufferUsageDefinition(Flags32 beginFlags)
		: m_beginFlags(beginFlags)
	{ }

	Flags32							m_beginFlags;
};

}