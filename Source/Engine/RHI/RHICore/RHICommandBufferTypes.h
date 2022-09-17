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
		: queueType(ECommandBufferQueueType::Graphics)
		, cmdBufferType(ECommandBufferType::Primary)
		, complexityClass(ECommandBufferComplexityClass::Default)
	{ }

	CommandBufferDefinition(ECommandBufferQueueType inQueueType, ECommandBufferType inCmdBufferType, ECommandBufferComplexityClass::Type inComplexityClass = ECommandBufferComplexityClass::Default)
		: queueType(inQueueType)
		, cmdBufferType(inCmdBufferType)
		, complexityClass(inComplexityClass)
	{ }

	ECommandBufferQueueType					queueType;
	ECommandBufferType						cmdBufferType;
	ECommandBufferComplexityClass::Type		complexityClass;
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
		: beginFlags(ECommandBufferBeginFlags::None)
	{ }

	explicit CommandBufferUsageDefinition(ECommandBufferBeginFlags inBeginFlags)
		: beginFlags(inBeginFlags)
	{ }

	ECommandBufferBeginFlags beginFlags;
};

} // spt::rhi