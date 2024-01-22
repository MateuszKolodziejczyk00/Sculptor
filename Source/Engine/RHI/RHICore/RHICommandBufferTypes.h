#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class ECommandBufferType
{
	Primary,
	Secondary
};


enum class EDeviceCommandQueueType
{
	Graphics,
	AsyncCompute,
	Transfer,
	NUM
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
		: queueType(EDeviceCommandQueueType::Graphics)
		, cmdBufferType(ECommandBufferType::Primary)
		, complexityClass(ECommandBufferComplexityClass::Default)
	{ }

	CommandBufferDefinition(EDeviceCommandQueueType inQueueType, ECommandBufferType inCmdBufferType, ECommandBufferComplexityClass::Type inComplexityClass = ECommandBufferComplexityClass::Default)
		: queueType(inQueueType)
		, cmdBufferType(inCmdBufferType)
		, complexityClass(inComplexityClass)
	{ }

	EDeviceCommandQueueType					queueType;
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


namespace std
{

template<>
struct hash<spt::rhi::CommandBufferDefinition>
{
    size_t operator()(const spt::rhi::CommandBufferDefinition& cmdBufferDef) const
    {
		return spt::lib::HashCombine(cmdBufferDef.queueType,
									 cmdBufferDef.cmdBufferType,
									 cmdBufferDef.complexityClass);
    }
};


} // std